# Game helpers

Free slot queries scan active flags in pool order. A full pool returns minus one.
City objects, items and objects have 1024 slots. Humans have 512 slots. Players have 256 slots.
Bullet removal moves the last active record into the gap and decreases the count.

Player actions use a ring of 64 entries. An item action stores two numbers.
A chat action copies the low six bits of the text length without a terminator.
Its mode comes from the voice mode control. Waypoints append three coordinates without changing their road fields.

Client account records contain a key and two random identifiers. A matching key returns its identifiers.
The store refuses new keys at 65535 entries. Account death tax takes five percent of the balance above 10000.
The multiply uses the double value of the float constant 0.05, then truncates to an integer.
Team cash changes wrap at 32 bits. Cash division assigns equal integer amounts to active team members.

Human update events obtain their recipient from the human's player field in the client.
The server supplies the recipient as an argument.
Client timer setup assigns 20 display timers starting at 64. Server timer setup first assigns 17 physics timers.
The client does not record those physics timers.

The socket state function closes the selected socket in the client. The server opens and binds it to the option port.
The client orientation encoder accepts unordered first and third plane tests as nonpositive values.
The two circle and segment queries accept unordered comparisons at their bounds. Server comparisons require ordered values.
The terrain circle query returns a Boolean in EAX. Its final loop index in EDX is not part of the result.
The clock test helper returns milliseconds as a 32-bit integer. An SSE2 conversion leaves EDX unchanged.
