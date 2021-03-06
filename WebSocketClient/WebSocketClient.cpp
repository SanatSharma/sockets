// WebSocketClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "WebSocketClient.h"
#include <uWS/uWS.h>
#include <agents.h>
#include <ppltasks.h>


/*
	Layout: 
	1) Take in StreamKey and MacAddr
	1) Call Server to get session
	2) Send connection request to websocket server
	3) 

*/
Concurrency::call<int> *call;
Concurrency::timer<int> *timer;
int StartQuiz();
int StartAttendance();
int StopAttendance();
int StopQuiz();
int NextQuestion();
void ErrorCheck(void* user);

const char* EventsStrings[] = { "Connection", "Message", "Disconnection", "Close" };
const char* ProfileStrings[] = { "rrq" };
const char* ActionStrings[] = { "start", "stop", "end", "next", "teacher", "classroom" };
const char* MessageStrings[] = { "profile", "type", "SessionID", "action", "RrqID", "qID", "wsID" };

const char* GetTextForEvent(int enumVal)
{
	return EventsStrings[enumVal];
}

const char* GetTextForMessage(int enumVal)
{
	return MessageStrings[enumVal];
}

const char* GetTextForProfile(int enumVal)
{
	return ProfileStrings[enumVal];
}

const char* GetTextForAction(int enumVal)
{
	return ActionStrings[enumVal];
}

const char* ATTENDANCE = "attendance";
const char* STOPATTENDANCE = "stopattendance";
int attendance_condition = 0;

int SESSION = 0;
int Q_ID = 0;
int RRQ_ID = -1;

int main(int argc, char *argv[]) {
	std::cout << "num arguments " << argc << std::endl;
	std::cout << "arguments " << argv << std::endl;
	if (argc > 0)
	{
		if (argc == 2)
		{
			SESSION = atoi(argv[1]);
 			std::cout << "Session is " << SESSION << std::endl;
		}
		else {
			MessageBoxA(0, "Something went wrong. Unable to get relevant session. Restart", "Notice", MB_ICONINFORMATION);
			SESSION = 4;  // only for debugging. Show Error in prod
		}
	}
	uWS::Hub h;
	std::mutex m;

	if (Quiz::startUp()) {
		std::cout << "Initialization is complete" << std::endl;
	}
	else {
		std::cout << "Initialization ERROR!" << std::endl;
		throw std::exception();
	}

	h.onConnection([](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest req) {
		// Get Session
		std::cout << "Connected!" << std::endl;
		// construct string to send to web server
		json j;
		j[GetTextForMessage(Message::PROFILE)] = GetTextForProfile(Profile::RRQ);
		j[GetTextForMessage(Message::TYPE)] = GetTextForEvent(Events::CONNECTION);
		j[GetTextForMessage(Message::ACTION)] = GetTextForAction(Action::CLASSROOMCONNECTION);
		j[GetTextForMessage(Message::SESSIONID)] = SESSION;

		std::string server_conn = j.dump();
		std::cout << server_conn << "\n";
		ws->send(server_conn.c_str());
	});

	h.onMessage([&m](uWS::WebSocket<uWS::CLIENT> *ws, char *message, size_t length, uWS::OpCode opCode)
	{
		using namespace std;
		cout << "Message received from server!" << endl;
		cout << string(message).substr(0, length) << endl;

		if (Utilities::IsJson(std::string(message).substr(0, length)))
		{
			json j = json::parse(std::string(message).substr(0, length));

			if (j.find(GetTextForMessage(Message::PROFILE)) != j.end()) {
				auto profile = j[GetTextForMessage(Message::PROFILE)].get<string>();

				if (profile.compare(string(GetTextForProfile(Profile::RRQ))) == 0) {
					auto action = j[GetTextForMessage(Message::ACTION)].get<string>();

					RRQ_ID = stoi(j[GetTextForMessage(Message::RRQID)].get<string>());
					Q_ID = j[GetTextForMessage(Message::QID)].get<int>();

					if (action.compare(string(GetTextForAction(Action::START))) == 0) {
						StartQuiz();
					}
					else if (action.compare(string(GetTextForAction(Action::STOP))) == 0) {
						StopQuiz();
					}
					else if (action.compare(string(GetTextForAction(Action::NEXT))) == 0) {
						NextQuestion();
					}
					else if (action.compare(string(GetTextForAction(Action::END))) == 0) {
						StopQuiz();
						RRQ_ID = -1;
					}
 				}
			}
		}
		else
		{
			cout << "we had an issue reading the JSON?" << endl;
		}
	});

	h.onDisconnection([](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *message, size_t length) {
		std::cout << "CLIENT CLOSE: " << code << std::endl;
	});

	h.connect("ws://localhost:3000");

	h.onError([](void *user) {
		ErrorCheck(user);
	});

	// callback function polls HID device for data
	std::function<void()> callback = [] {
		if (attendance_condition) {
			printf("Attendance poll: %d\n", attendance_condition);
			Quiz::quiz_poll(1);
		}
		else
			Quiz::quiz_poll();
	};

	call = new concurrency::call<int>(
		[callback](int)
	{
		callback();
	});

	// create a concurrent timer that polls the HID every time_ms ms
	// this is important since it leaves the main thread open to receive calls on WebSocket
	int time_ms = 500; // default poll time 500ms
	timer = new concurrency::timer<int>(time_ms, 0, call, true); 

	h.run();

	// The following code is run when the program shuts down

	std::cout << "*************************\n";
	timer->stop();
	delete timer; // delete timer object to free memory
	Quiz::session_end();
	getchar();
}

int StartQuiz() {
	if (RRQ_ID == -1)
	{
		// Need to get RRQ ID
		std::cout << "Need to get RRQ_ID\n";
		RRQ_ID = Rest::getRRQ(SESSION);
	}
	std::cout << "Time to start quiz" << std::endl;
	timer->start();
	if (!Quiz::quiz_start())
	{
		Quiz::quiz_stop();
		Quiz::quiz_start();
	}
	return 1;
}

int StartAttendance() {
	std::cout << "Start attendance" << std::endl;
	timer->start();
	attendance_condition = 1;
	printf("Setting attendance condition = 1\n");
	if (!Quiz::quiz_start())
	{
		Quiz::quiz_stop();
		Quiz::quiz_start();
	}
	return 1;
}

int StopAttendance() {
	std::cout << "Stop attendance" << std::endl;
	attendance_condition = 0;
	printf("Setting attendance condition = 0\n");
	timer->stop();
	if (!Quiz::quiz_stop())
	{
		Quiz::quiz_start();
		Quiz::quiz_stop();
	}
	return 1;
}

int StopQuiz() {
	std::cout << "Stop quiz" << std::endl;
	timer->pause();
	if (!Quiz::quiz_stop())
	{
		Quiz::quiz_start();
		Quiz::quiz_stop();
	}
	return 1;
}

int NextQuestion() {
	std::cout << "Next Question" << std::endl;
	timer->start();
	if (!Quiz::quiz_start())
	{
		Quiz::quiz_stop();
		Quiz::quiz_start();
	}
	return 1;
}

void ErrorCheck(void* user) {
	int protocolErrorCount = 0;
	switch ((long)user) {
	case 1:
		std::cout << "Client emitted error on invalid URI" << std::endl;
		getchar();
		break;
	case 2:
		std::cout << "Client emitted error on resolve failure" << std::endl;
		getchar();
		break;
	case 3:
		std::cout << "Client emitted error on connection timeout (non-SSL)" << std::endl;
		getchar();
		break;
	case 5:
		std::cout << "Client emitted error on connection timeout (SSL)" << std::endl;
		getchar();
		break;
	case 6:
		std::cout << "Client emitted error on HTTP response without upgrade (non-SSL)" << std::endl;
		getchar();
		break;
	case 7:
		std::cout << "Client emitted error on HTTP response without upgrade (SSL)" << std::endl;
		getchar();
		break;
	case 10:
		std::cout << "Client emitted error on poll error" << std::endl;
		getchar();
		break;
	case 11:
		protocolErrorCount++;
		std::cout << "Client emitted error on invalid protocol" << std::endl;
		if (protocolErrorCount > 1) {
			std::cout << "FAILURE:  " << protocolErrorCount << " errors emitted for one connection!" << std::endl;
			getchar();
		}
		break;
	default:
		std::cout << "FAILURE: " << user << " could not connect to websocket server" << std::endl;
		getchar();
	}
}