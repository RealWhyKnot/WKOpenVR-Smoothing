#include "IPCServer.h"
#include "Logging.h"
#include "ServerTrackedDeviceProvider.h"

void IPCServer::HandleRequest(const protocol::Request &request, protocol::Response &response)
{
	switch (request.type)
	{
	case protocol::RequestHandshake:
		response.type = protocol::ResponseHandshake;
		response.protocol.version = protocol::Version;
		break;

	case protocol::RequestSetConfig:
		driver->SetConfig(request.setConfig);
		response.type = protocol::ResponseSuccess;
		break;

	default:
		LOG("Invalid IPC request: %d", request.type);
		break;
	}
}

IPCServer::~IPCServer()
{
	Stop();
}

void IPCServer::Run()
{
	mainThread = std::thread(RunThread, this);
}

void IPCServer::Stop()
{
	TRACE("IPCServer::Stop()");
	if (!running)
		return;

	stop = true;
	SetEvent(connectEvent);
	mainThread.join();
	running = false;

	if (connectEvent) {
		CloseHandle(connectEvent);
		connectEvent = nullptr;
	}

	TRACE("IPCServer::Stop() finished");
}

IPCServer::PipeInstance *IPCServer::CreatePipeInstance(HANDLE pipe)
{
	// Value-init: PipeInstance contains an OVERLAPPED whose hEvent must be NULL
	// for the completion-routine variants of WriteFileEx/ReadFileEx. Default-init
	// (`new PipeInstance`) leaves OVERLAPPED with heap-residual garbage; some
	// resident bytes in hEvent cause the API to fail.
	auto pipeInst = new PipeInstance{};
	pipeInst->pipe = pipe;
	pipeInst->server = this;
	pipes.insert(pipeInst);
	return pipeInst;
}

void IPCServer::ClosePipeInstance(PipeInstance *pipeInst)
{
	DisconnectNamedPipe(pipeInst->pipe);
	CloseHandle(pipeInst->pipe);
	pipes.erase(pipeInst);
	delete pipeInst;
}

void IPCServer::RunThread(IPCServer *_this)
{
	_this->running = true;
	LPCTSTR pipeName = TEXT(FINGERSMOOTHING_PIPE_NAME);

	HANDLE connectEvent = _this->connectEvent = CreateEvent(0, TRUE, TRUE, 0);
	if (!connectEvent)
	{
		LOG("CreateEvent failed in RunThread. Error: %d", GetLastError());
		return;
	}

	OVERLAPPED connectOverlap;
	connectOverlap.hEvent = connectEvent;

	HANDLE nextPipe;
	BOOL connectPending = CreateAndConnectInstance(&connectOverlap, nextPipe);

	while (!_this->stop)
	{
		DWORD wait = WaitForSingleObjectEx(connectEvent, INFINITE, TRUE);

		if (_this->stop)
		{
			break;
		}
		else if (wait == 0)
		{
			if (connectPending)
			{
				DWORD bytesConnect;
				BOOL success = GetOverlappedResult(nextPipe, &connectOverlap, &bytesConnect, FALSE);
				if (!success)
				{
					LOG("GetOverlappedResult failed in RunThread. Error: %d", GetLastError());
					return;
				}
			}

			LOG("IPC client connected");

			auto pipeInst = _this->CreatePipeInstance(nextPipe);
			CompletedWriteCallback(0, sizeof protocol::Response, (LPOVERLAPPED) pipeInst);

			connectPending = CreateAndConnectInstance(&connectOverlap, nextPipe);
		}
		else if (wait != WAIT_IO_COMPLETION)
		{
			printf("WaitForSingleObjectEx failed in RunThread. Error %d", GetLastError());
			return;
		}
	}

	for (auto &pipeInst : _this->pipes)
	{
		_this->ClosePipeInstance(pipeInst);
	}
	_this->pipes.clear();
}

BOOL IPCServer::CreateAndConnectInstance(LPOVERLAPPED overlap, HANDLE &pipe)
{
	pipe = CreateNamedPipe(
		TEXT(FINGERSMOOTHING_PIPE_NAME),
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		PIPE_UNLIMITED_INSTANCES,
		sizeof protocol::Request,
		sizeof protocol::Response,
		1000,
		0
	);

	if (pipe == INVALID_HANDLE_VALUE)
	{
		LOG("CreateNamedPipe failed. Error: %d", GetLastError());
		return FALSE;
	}

	ConnectNamedPipe(pipe, overlap);

	switch(GetLastError())
	{
	case ERROR_IO_PENDING:
		return TRUE;

	case ERROR_PIPE_CONNECTED:
		if (SetEvent(overlap->hEvent))
			return FALSE;
	}

	LOG("ConnectNamedPipe failed. Error: %d", GetLastError());
	CloseHandle(pipe);
	pipe = INVALID_HANDLE_VALUE;
	return FALSE;
}

void IPCServer::CompletedReadCallback(DWORD err, DWORD bytesRead, LPOVERLAPPED overlap)
{
	PipeInstance *pipeInst = (PipeInstance *) overlap;
	BOOL success = FALSE;

	if (err == 0 && bytesRead > 0)
	{
		pipeInst->server->HandleRequest(pipeInst->request, pipeInst->response);
		success = WriteFileEx(
			pipeInst->pipe,
			&pipeInst->response,
			sizeof protocol::Response,
			overlap,
			(LPOVERLAPPED_COMPLETION_ROUTINE) CompletedWriteCallback
		);
	}

	if (!success)
	{
		if (err == ERROR_BROKEN_PIPE)
		{
			LOG("IPC client disconnecting normally");
		}
		else
		{
			LOG("IPC client disconnecting due to error (via CompletedReadCallback), error: %d, bytesRead: %d", err, bytesRead);
		}
		pipeInst->server->ClosePipeInstance(pipeInst);
	}
}

void IPCServer::CompletedWriteCallback(DWORD err, DWORD bytesWritten, LPOVERLAPPED overlap)
{
	PipeInstance *pipeInst = (PipeInstance *) overlap;
	BOOL success = FALSE;

	if (err == 0 && bytesWritten == sizeof protocol::Response)
	{
		success = ReadFileEx(
			pipeInst->pipe,
			&pipeInst->request,
			sizeof protocol::Request,
			overlap,
			(LPOVERLAPPED_COMPLETION_ROUTINE) CompletedReadCallback
		);
	}

	if (!success)
	{
		LOG("IPC client disconnecting due to error (via CompletedWriteCallback), error: %d, bytesWritten: %d", err, bytesWritten);
		pipeInst->server->ClosePipeInstance(pipeInst);
	}
}
