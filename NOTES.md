Here's what I would recommend:

First off, opening a raw socket is dead simple. Just do:
int fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
And you now have a raw socket ready to send/receive ICMP packets. :)
Note that you may need root privileges for this (depending on how your
system is configured), socket() will fail otherwise.

Now..

1. Run "ping some.host" in one terminal
2. Run "tcpdump -n -XX icmp" in another terminal

You should now be able to see both the exact contents of both the
request and response packets.
(There might be some other noise in case other things in your network
are pinging each other at the same time, but usually it's pretty
quiet.)

Now that you know what the packets should look like in both directions..

3. Write an echo client that performs the following steps:
3a. Open a raw socket
3b. Parse the target address from argv[1] (or wherever you get it
from) using e.g gethostbyname()
3c. Construct an ICMP Echo Request packet
3d. sendto() the packet on the raw socket to the target address from 3b
3e. recvfrom() on the raw socket until you get an ICMP Echo Response packet
3f. Print out something nice :)
3g. Goto 3c., keep going until the user interrupts

Note: raw sockets receive a bunch of unfiltered traffic, the kernel
doesn't know what you want so it just gives you everything from the
network. It's up to you to filter it and make sense of incoming
packets.

Also note: you don't need to bind() or connect() a raw socket, you can
just start using sendto() and recvfrom() right away :)
