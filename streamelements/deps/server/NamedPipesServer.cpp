#ifdef _WIN32

#include "NamedPipesServer.hpp"

#include <obs.h>

#include <algorithm>

#pragma comment(lib, "advapi32.lib")
#include <accctrl.h>
#include <aclapi.h>

static const size_t BUFFER_SIZE = 512;
static const int CLIENT_TIMEOUT_MS = 5000;

NamedPipesServer::NamedPipesServer(
	const char* const pipeName,
	NamedPipesServerClientHandler::msg_handler_t msgHandler,
	size_t maxClients) :
	m_pipeName(pipeName),
	m_msgHandler(msgHandler),
	m_maxClients(maxClients),
	m_running(false)
{
	os_event_init(&m_done_event, OS_EVENT_TYPE_MANUAL);
}

NamedPipesServer::~NamedPipesServer()
{
	Stop();

	os_event_destroy(m_done_event);

	m_done_event = nullptr;
}

void NamedPipesServer::Start()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (IsRunning()) {
		return;
	}

	os_atomic_store_bool(&m_running, true);
	os_event_reset(m_done_event);

	m_thread = std::thread([this]() {
		ThreadProc();
	});
}

void NamedPipesServer::Stop()
{
	{
		std::lock_guard<std::recursive_mutex> guard(m_mutex);

		if (!IsRunning()) {
			return;
		}

		os_atomic_store_bool(&m_running, false);
	}

	while (os_event_timedwait(m_done_event, 100) != 0)
	{
		// Attempt to connect to release the server thread
		HANDLE hPipe = ::CreateFileA(m_pipeName.c_str(),
					     GENERIC_READ | GENERIC_WRITE, 0,
					     NULL, OPEN_EXISTING, 0, NULL);

		if (hPipe != INVALID_HANDLE_VALUE) {
			::CloseHandle(hPipe);
		} else {
			break;
		}
	}

	if (m_thread.joinable()) {
		m_thread.join();
	}
}

bool NamedPipesServer::IsRunning()
{
	return os_atomic_load_bool(&m_running);
}

bool NamedPipesServer::CanAddHandler()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return (m_clients.size() < m_maxClients);
}

void NamedPipesServer::RemoveDisconnectedClients()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	std::vector<NamedPipesServerClientHandler*> removeClients;

	for (auto client : m_clients) {
		if (!client->IsConnected()) {
			removeClients.push_back(client);
		}
	}

	for (auto client : removeClients) {
		m_clients.remove(client);

		delete client;
	}
}

void NamedPipesServer::DisconnectAllClients()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	for (auto client : m_clients) {
		client->Disconnect();
	}
}

void NamedPipesServer::WriteMessage(const char* const buffer, const size_t length)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	for (auto client : m_clients) {
		client->WriteMessage(buffer, length);
	}

	RemoveDisconnectedClients();
}

void NamedPipesServer::ThreadProc()
{
	while (IsRunning() || !m_clients.empty()) {
		RemoveDisconnectedClients();

		if (!IsRunning()) {
			DisconnectAllClients();
			RemoveDisconnectedClients();

			break;
		}

		if (!CanAddHandler()) {
			Sleep(CLIENT_TIMEOUT_MS);
		}
		else {
			HANDLE hPipe = INVALID_HANDLE_VALUE;
			PSID pEveryoneSID = NULL;
			SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;

			if (AllocateAndInitializeSid(&SIDAuthWorld, 1,
				SECURITY_WORLD_RID,
				0, 0, 0, 0, 0, 0, 0,
				&pEveryoneSID)) {
				EXPLICIT_ACCESS ea[2];

				// Initialize an EXPLICIT_ACCESS structure for an ACE.
				// The ACE will allow Everyone read access to the key.
				ZeroMemory(&ea, 2 * sizeof(EXPLICIT_ACCESS));
				ea[0].grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
				ea[0].grfAccessMode = SET_ACCESS;
				ea[0].grfInheritance = NO_INHERITANCE;
				ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
				ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
				ea[0].Trustee.ptstrName = (LPTSTR)pEveryoneSID;

				PACL pACL = NULL;

				if (ERROR_SUCCESS == SetEntriesInAcl(1, ea, NULL, &pACL)) {
					PSECURITY_DESCRIPTOR pSD =
						(PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);

					if (pSD != NULL) {
						if (InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION)) {
							if (SetSecurityDescriptorDacl(pSD, TRUE, pACL, FALSE)) {
								SECURITY_ATTRIBUTES sa;

								// Initialize a security attributes structure.
								sa.nLength = sizeof(SECURITY_ATTRIBUTES);
								sa.lpSecurityDescriptor = pSD;
								sa.bInheritHandle = FALSE;

								hPipe = ::CreateNamedPipeA(
									m_pipeName.c_str(),
									PIPE_ACCESS_DUPLEX,
									PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
									m_maxClients,
									BUFFER_SIZE,
									BUFFER_SIZE,
									CLIENT_TIMEOUT_MS,
									&sa);
							}
						}

						LocalFree(pSD);
					}

					LocalFree(pACL);
				}

				FreeSid(pEveryoneSID);
			}

			if (hPipe == INVALID_HANDLE_VALUE) {
				blog(LOG_ERROR, "obs-streamelements-core: NamedPipesServer: CreateNamedPipe failed: %d", GetLastError());

				Sleep(CLIENT_TIMEOUT_MS);
			} else {
				const bool isConnected =
					ConnectNamedPipe(hPipe, NULL)
						? TRUE
						: (GetLastError() ==
						   ERROR_PIPE_CONNECTED);

				if (isConnected) {
					if (IsRunning()) {
						blog(LOG_INFO,
						     "obs-streamelements-core: NamedPipesServer: ConnectNamedPipe: client connected");

						m_clients.push_back(
							new NamedPipesServerClientHandler(
								hPipe,
								m_msgHandler));
					} else {
						CloseHandle(hPipe);
					}
				} else {
					CloseHandle(hPipe);
				}
			}
		}
	}

	os_atomic_store_bool(&m_running, false);

	os_event_signal(m_done_event);
}

#endif
