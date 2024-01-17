The project consists of a general TCP proxy which works as a Man-In-The-Middle for the client-server connection.

At the beginning of the application the proxy will get information about the ip addresses and the mac addresses it needs to block, the bytes it needs to replace and their substitutes, as well as regex syntaxes for known ip protocols that don't need extra analysis and can be directly forwarded.

Each message (sent either by the server or the proxy) is intercepted by the proxy and stopped until it decides what to do with it. 

Firstly, the proxy will check if the received protocol is part of the white protocols from the rules file. If it is, it will be forwarded immediately, if not it will go under analysis.

Every message sent will firstly have its protocol displayed (if it is a protocol recognized by the proxy) and its hexdump.

In the scope of a user-friendly environment, we build a user guide to every step. The proxy user will have to take a decision for each packet he gets:
	- F = forward the packet with no additional interventions
	- D = drop the packet (with no consequences to the connection)
	- R = replace the bytes you can find in the rules file
	- C = replace custom bytes 
		-> first you send the sequence of bytes you want to be replaced (no whitespaces addded)
		-> then you send the sequence of bytes you want to be replaced with (if you want them to be deleted press ENTER)

Every action will be documented in the internal history.log.

Other aspects of the project:
	If the proxy protocol receives "-F" as a parameter it will forward all packets directly.
	For the implementation of multiple client handling, we build the application as multi-threading using a global mutex to define the access to the critical zone.



Functions used in code and their actions:

void getRules(const char* filepath)
	-> gets the ip and mac addresses for the black list, the replacement rules for bytes (in the idea of malicious code protection), the white list protocols in regex format

void *handle_client(void *arg) 
	-> main function which generates the actions taken on the received message
	-> is sent as parameter to pthread_create 

int protIsValid(char* message)
	-> tries to match the message to a protocol in the white list structure
	-> returns -1 in case of errors, 1 if it has a match, 0 if it needs to be analysed further
	
void hex_dump(const char* message, Client *c)
	-> prints the message in hexdump format
	
char* charToHex(const char* input)
	-> transforms a given message from clear text to HEX format
	-> returns either NULL for errors or a char* for the result
	
int hex_to_int(char c)
	-> intermediate function used in the converstion from HEX to ASCII messages
	-> returns the ASCII code associated with the character sent as a parameter

char* hexToAscii(char* hex, int hex_len)
	-> converts a given message from HEX to ASCII
	-> returns either NULL for errors or a char* for the result

char* replaceCustomBytes(const char* message, const char* bytes2Replace, const char* replacement, int* lenght)
	-> function which takes a message, a sequence of bytes to be replaced and a sequence of bytes as replacement
	-> it searches through the message until it can match a sequence of bytes to the given one and it starts replacing the bytes with the replacement.
	-> if the replacement is empty it will delete the bytes received to be replaced

void initialize_protocols()
	-> initializes the structure for known protocols with regex and specific bytes syntaxes to match for protocol detection
	
char* whatprotocol(char *message, Client *c) 
	-> matches the message to a protocol based on port numbers and regex

char* printType(char* message)
	-> determines the protocol type based on bytes matching 

int searchBlocked(char *add, int type)
	-> searches for blocked addresses
	
void history(const char* maker, const char* action) 
	-> adds time-stamped actions to the history.log
