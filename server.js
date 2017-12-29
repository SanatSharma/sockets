const WebSocket = require('ws');

const wss = new WebSocket.Server({ port: 3000 });
console.log("Here");

wss.on('connection', function connection(ws, req) {
  const ip = req.connection.remoteAddress;
  wss.clients.add(ws)
  ws._socket.setKeepAlive(true);

  console.log("Added client!");

  ws.on("close", function(){
    console.log("removing client " + ws);
    wss.clients.delete(ws);
  });

  ws.on("message", function ws_message(data){
    console.log("Data: " + data);
    // Broadcast message to all the centers
    wss.clients.forEach(function each(client){
        if (client !== ws && client.readyState == WebSocket.OPEN){
          client.send(data);
        }
    });
  });
  // Handle any errors that occur.
  ws.on("error", function(error) {
    console.log('WebSocket Error: ' + error);
  });
});

