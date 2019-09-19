# TCP-chat-server
# A chat application based on the client-server model
# All clients communicate with each other through the server, and the server will forward message from one client to another.
# The server is able to handle multiple clients at the same time, using the select system call
# The server maintains a recod of all the ClientIDs currently participating and is able to handle the following type of message
  # A HELLO message: clients tell the server that they are connected
    - used by new clients as well as those that were disconnected earlier and want to connect again.
    - When the server receives a HELLO message, it will register the client by keeping track of its ClientID 
    and send back a HELLO_ACK message.    
  # LIST_REQUEST message
    - Client can request for the list of connected clients
  # CHAT message
    - Clients can send messages to other clients by sending a CHAT message to the server
    - The server will then forward this message to the intended recipient in another CHAT message
  # EXIT message
    - Signals that the client is leaving.
    
# Every message has a header followed by an optional data part.
  The header has the following format:
  1) Type: short int (2 bytes)
  Values defined later.
  2) source: char[20], null terminated string specifying the source. message originating from
  server uses the id “Server”
  3) destination: char[20], its semantics are the same as the source field. Message destined
  to the server should have the id “Server”.
  4) Length: int (4 bytes), specifies the length of the data part. Value should be zero if there is
  no data part.
  5) Message-ID: int (4 bytes), this is set by the client for every chat message. The server
  echoes it back in case the message wasn’t delivered i.e., ERROR(CANNOT_DELIVER)
  scenario. This allows the client to identify which message wasn’t delivered ( in case the
  client wants to retransmit). The default value is zero and for chat message a value >= 1
  should be used.
