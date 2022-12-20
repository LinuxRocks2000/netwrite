# netwrite
A simple network-based version of `write` in C

Compiled the server with `gcc netwrite-server.c -o server` and the client with `gcc netwrite.c -o client`. Port is 6780; making it changeable is on the list.

The server is a mild security risk as it technically allows external actors to write to arbitrary TTYs. In practice, however, it is easy to avoid this kind of threat with a firewall or by just not exposing port 6780 to the rest of the internet, whichever one applies for you.

Use with `./client username@host`.

At the moment there is no `respond` feature. That is the next thing I intend to add.
