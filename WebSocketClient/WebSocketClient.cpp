// WebSocketClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <uWS/uWS.h>
#include "Quiz.h"
#include <agents.h>
#include <ppltasks.h>
#include "Sql.h"
#include "Rest.h"
#include <nlohmann/json.hpp>

Concurrency::call<int> *call;
Concurrency::timer<int> *timer;
const char* START = "start";
const char* NEXT = "next";
const char* STOP = "stop";
const char* ATTENDANCE = "attendance";
const char* STOPATTENDANCE = "stopattendance";
int attendance_condition = 0;
int SESSION;
int QID;
int RRQ_ID;
using json = nlohmann::json;

/*
	Layout: 
	1) Take in StreamKey and MacAddr
	1) Call Server to get session
	2) Send connection request to websocket server
	3) 

*/

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
	}
	uWS::Hub h;
	std::mutex m;

	if (Quiz::startUp()) {
		std::cout << "Initialization is complete" << std::endl;
		// connect to sql databases
		//if (!sql_connect())
		//	throw std::exception();
		//if (!attendance_init())
		//	throw std::exception();
	}
	else {
		std::cout << "Initialization ERROR!" << std::endl;
		throw std::exception();
	}

	h.onConnection([](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest req) {

		// Get Session
		//assert(macAddr != nullptr);
		//assert(streamKey != nullptr);
		//SESSION = Rest::getSession(macAddr, streamKey);

		std::cout << "Connected!" << std::endl;
		// construct string to send to web server
		json j;
		j["session"] = SESSION;
		j["type"] = "center";
		std::string test = j.dump();
		std::cout << test << "\n";

		std::string data = "{";
		data.append("\"session\":\"00001\",");
		data.append("\"type\":\"center\"");
		data.append("}");	
		ws->send(test.c_str());
	});

	h.onMessage([&m](uWS::WebSocket<uWS::CLIENT> *ws, char *message, size_t length, uWS::OpCode opCode) {
		std::cout << "Message received!" << std::endl;
		if (strncmp(message, START, strlen(START)) == 0) {
			std::cout << "Time to start quiz" << std::endl;
			timer->start();
			if (!Quiz::quiz_start()) {
				Quiz::quiz_stop();
				Quiz::quiz_start();
			}
		}
		else if (strncmp(message, ATTENDANCE, strlen(ATTENDANCE)) == 0) {
			std::cout << "Start attendance" << std::endl;
			timer->start();
			attendance_condition = 1;
			printf("Setting attendance condition = 1\n");
			if (!Quiz::quiz_start()) {
				Quiz::quiz_stop();
				Quiz::quiz_start();
			}
		}
		else if (strncmp(message, STOPATTENDANCE, strlen(STOPATTENDANCE)) == 0) {
			std::cout << "Stop attendance" << std::endl;
			attendance_condition = 0;
			printf("Setting attendance condition = 0\n");
			timer->stop();
			if (!Quiz::quiz_stop()) {
				Quiz::quiz_start();
				Quiz::quiz_stop();
			}
		}
		else if (strncmp(message, NEXT, strlen(NEXT)) == 0) {
			std::cout << "Next Question" << std::endl;
			timer->start();
			if (!Quiz::quiz_start()) {
				Quiz::quiz_stop();
				Quiz::quiz_start();
			}
		}
		else if (strncmp(message, STOP, strlen(STOP)) == 0) {
			std::cout << "Stop quiz" << std::endl;
			timer->pause();
			if (!Quiz::quiz_stop()) {
				Quiz::quiz_start();
				Quiz::quiz_stop();
			}
		}
		
	});

	h.onDisconnection([](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *message, size_t length) {
		std::cout << "CLIENT CLOSE: " << code << std::endl;
	});

	h.connect("ws://localhost:3000");

	h.onError([](void *user) {
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
			std::cout << "FAILURE: " << user << " should not emit error!" << std::endl;
			getchar();
		}
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
	sql_close();
	getchar();
}

