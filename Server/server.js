'use strict';
exports.__esModule = true;
/*
enum Events {
    CONNECTION = "Connection",
    MESSAGE = "Message",
    DISCONNECTION = "Disconnect",
    CLOSE = "Close"
}

enum Profile {
    RRQ = "rrq",
}

enum Action {
    START = "start",
    STOP = "stop",
    END = "end",
    NEXT = "next",
    TEACHERCONNECTION = "teacher",
    CLASSROOMCONNECTION = "classroom"
}

enum Message {
    PROFILE = 'profile',
    TYPE = 'type',
    SESSIONID = 'SessionID',
    ACTION = 'action',
    RRQID = "RrqID",
    QID = "qID",
    WSID = "wsID"
}

Teacher:
        Initiate ws-session with the following connection
        {
            profile: Profile.RRQ,
            type: Event.CONNECTION,
            action: Action.TEACHERCONNECTION
            SessionID: <session id>
        }

        Add to Websockets map, teacher connections
        If session exists in Connections list, update the wsID of teacher for that connection
        Else create the connection for that session

Classroom:
        Receive Message from Classrooms wanting to join
        {
            profile: Profile.RRQ,
            type: EVENT.CONNECTION,
            action: Action.CLASSROOMCONNECTION
            SessionID: <session id>
        }
        Add to websockets map, classrooms map, classroomWSids for that session connection

        Upon adding classroom, send it a message so that it knows its connected
        {
            profile: Profile.RRQ,
            action: Event.CONNECTION
        }
        
Teacher:
        Message:
            {
                profile: Profile.RRQ,
                type: Event.MESSAGE,
                action: Action.START/STOP/NEXT/END,
                RrqID: <rrq id>,
                qID: <question id>
                SessionID: <session id>
            }
        On actions Stop and End, RrqID and qID are not consequential and can blank

        Send the following to the classrooms:
            {
                profile: Profile.RRQ,
                action: Action.START/STOP/NEXT/END,
                RrqID: <rrq id>,
                qID: <question id>
            }
        
        Close Connection:

          We Wish to close connection at the end of the session. How do we accomplish this?
          Furthermore, we wish to create one websocket connection and maintain it even when the RRQ page is closed
        
          onClose/disconnection:
            1) Teacher: Remove from teacher connections and websocket connections
            2) Classroom: Remove from classroom connections, websocket connections and connection classroomWSids
          
          On session close:
            {
              profile: Profile.RRQ,
              type: Event.CLOSE,
              SessionID: <session id>
            }

            Do the following at server:
            1) Close classroom websocket connections and delete from ws dictionary
            2) Delete connection object from Connections list
            3) Close Teacher Connection and delete from ws dictionary

*/
var Teacher = /** @class */ (function () {
    function Teacher(wsID, conn) {
        this.wsID = wsID;
        this.conn = conn;
    }
    return Teacher;
}());
var Classroom = /** @class */ (function () {
    function Classroom(wsID, conn) {
        this.wsID = wsID;
        this.conn = conn;
    }
    return Classroom;
}());
var Connection = /** @class */ (function () {
    function Connection(SessionID, wsID) {
        this.SessionID = SessionID;
        this.wsID = wsID;
        this.classroomWsID = [];
    }
    return Connection;
}());
var Events;
(function (Events) {
    Events["CONNECTION"] = "Connection";
    Events["MESSAGE"] = "Message";
    Events["DISCONNECTION"] = "Disconnect";
    Events["CLOSE"] = "Close";
})(Events || (Events = {}));
var Profile;
(function (Profile) {
    Profile["RRQ"] = "rrq";
})(Profile || (Profile = {}));
var Action;
(function (Action) {
    Action["START"] = "start";
    Action["STOP"] = "stop";
    Action["END"] = "end";
    Action["NEXT"] = "next";
    Action["TEACHERCONNECTION"] = "teacher";
    Action["CLASSROOMCONNECTION"] = "classroom";
})(Action || (Action = {}));
var Message;
(function (Message) {
    Message["PROFILE"] = "profile";
    Message["TYPE"] = "type";
    Message["SESSIONID"] = "SessionID";
    Message["ACTION"] = "action";
    Message["RRQID"] = "RrqID";
    Message["QID"] = "qID";
    Message["WSID"] = "wsID";
})(Message || (Message = {}));
var fs = require("fs");
var util = require("util");
var log_file = fs.createWriteStream(__dirname + '/debug.log', { flags: 'w' });
var log_stdout = process.stdout;
console.log = function (d) {
    d = new Date().toString() + " | " + d;
    log_file.write(util.format(d) + '\n');
    log_stdout.write(util.format(d) + '\n');
};
var WebSocket = require("ws");
var connections = {};
var websockets = {}; // websocket mapping
var teachers = {};
var classrooms = {};
var port = 3000;
var idCounter = 0;
var wss = new WebSocket.Server({ port: port });
wss.on('connection', function connection(ws) {
    ws.on('close', function () {
        try {
            console.log("Handling Disconnection");
            HandleDisconnection(ws);
        }
        catch (e) {
            console.error(e);
        }
    });
    ws.on('message', function (message) {
        try {
            var json = JSON.parse(message);
            HandleMessage(ws, json);
        }
        catch (e) {
            console.error("Exception: " + e);
        }
    });
    ws.on("error", function (error) {
        console.error('WebSocket Error: ' + error);
    });
});
function assert(condition, message) {
    if (!condition) {
        message = message || "Assertion failed";
        if (typeof Error !== "undefined") {
            throw new Error(message);
        }
        throw message; // Fallback
    }
}
function HandleDisconnection(ws) {
    if (ws.id in teachers) {
        console.log("Teacher disconnection");
        var teacherWsID = ws.id;
        var Teacher = teachers[teacherWsID];
        delete websockets[teacherWsID];
        delete teachers[teacherWsID];
    }
    else if (ws.id in classrooms) {
        console.log("Removing Classroom connection. Number of Connections is " + (Object.keys(classrooms).length - 1));
        var classroomWsID = ws.id;
        var classroom = classrooms[classroomWsID];
        var conn = classroom.conn;
        var index = -1;
        for (var i = 0; i < conn.classroomWsID.length; i++) {
            if (classroomWsID === conn.classroomWsID[i]) {
                index = i;
                break;
            }
        }
        if (index === -1) {
            throw "Classroom not in Connection Classroom list";
        }
        else {
            conn.classroomWsID.splice(index, 1);
        }
        delete classrooms[classroomWsID];
        delete websockets[classroomWsID];
    }
    else {
        throw "Member neither in teacher nor classroom connections";
    }
}
function CloseConnection(wsID, json) {
    var SessionID = json[Message.SESSIONID];
    console.log("Closing Session: " + SessionID);
    assert(SessionID in connections, "Session not present in Connections");
    var conn = connections[SessionID];
    for (var _i = 0, _a = conn.classroomWsID; _i < _a.length; _i++) {
        var wsID = _a[_i];
        websockets[wsID].close();
        delete websockets[wsID];
        delete classrooms[wsID];
    }
    delete teachers[conn.wsID];
    delete websockets[conn.wsID];
    delete connections[SessionID];
}
function HandleMessage(ws, json) {
    assert(Message.PROFILE in json, "Profile non existent in message");
    if (json[Message.PROFILE] === Profile.RRQ) {
        if (json[Message.TYPE] == Events.CONNECTION) {
            ws.id = idCounter++;
            if (json[Message.ACTION] === Action.TEACHERCONNECTION) {
                AddTeacher(ws, json);
            }
            else if (json[Message.ACTION] === Action.CLASSROOMCONNECTION) {
                AddClassroom(ws, json);
            }
        }
        else if (json[Message.TYPE] == Events.MESSAGE) {
            assert(ws.id in teachers, "Illegal Entity sending message. Do not send!");
            console.log("Message: " + JSON.stringify(json));
            SendMessage(ws.id, json);
        }
        else if (json[Message.TYPE] == Events.CLOSE) {
            CloseConnection(ws.id, json);
        }
    }
}
function AddTeacher(ws, json) {
    var teacherWsID = ws.id;
    var SessionID = json[Message.SESSIONID];
    console.log("Adding new Teacher and connection " + teacherWsID);
    if (SessionID in connections) {
        console.log("Session connection already exists");
        var conn = connections[SessionID];
        if (conn.wsID in teachers) {
            throw "Teacjer connection already exists. Rejecting Teacher Connection!";
        }
        conn.wsID = teacherWsID;
    }
    else {
        var conn = new Connection(json[Message.SESSIONID], teacherWsID);
        connections[SessionID] = conn;
        console.log("Number of Connections: " + Object.keys(connections).length);
    }
    var teacher = new Teacher(teacherWsID, conn);
    teachers[teacherWsID] = teacher; // Add to Teachers dict
    websockets[teacherWsID] = ws; // Add to websockets dict
}
function AddClassroom(ws, json) {
    var classroomWsID = ws.id;
    console.log("Adding new classroom " + classroomWsID + ". Number of classrooms " + (Object.keys(classrooms).length + 1));
    var SessionID = json[Message.SESSIONID];
    assert(SessionID in connections, "Could not add Classroom. Connection channel non existent!");
    var conn = connections[SessionID];
    conn.classroomWsID.push(classroomWsID); // Add to channel's classroom list
    console.log("Added classroom to connection list. " + conn.classroomWsID);
    var classroom = new Classroom(classroomWsID, conn);
    classrooms[classroomWsID] = classroom; // Add to classrooms dict
    websockets[classroomWsID] = ws; // Add to websockets dict
    // Send confirmation to classroom
    ws.send(JSON.stringify({
        profile: Profile.RRQ,
        action: Events.CONNECTION
    }));
}
function SendMessage(teacherWsID, json) {
    var teacher = teachers[teacherWsID];
    var conn = teacher.conn;
    for (var _i = 0, _a = conn.classroomWsID; _i < _a.length; _i++) {
        var wsID = _a[_i];
        var ws = websockets[wsID];
        ws.send(JSON.stringify({
            profile: Profile.RRQ,
            type: json[Message.TYPE],
            action: json[Message.ACTION],
            RrqID: json[Message.RRQID],
            qID: json[Message.QID]
        }));
    }
}
