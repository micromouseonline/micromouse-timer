/***
 *
 * The transmission sequence begins by setting the radio data pin high so that carrier is
 * transmitted immediately after the transmitter is woken up.
 *
 * To wake up the radio, it must first be powered up and then switched into transmit mode.
 *
 *
 * A short delay is needed for the transmitter to stabilise. According to the datasheet for
 * the TRM-433-LT module, the carrier should be ready within 500 microseconds.
 *
 * Delaying too long after enabling the transmitter can cause receive errors.
 *
 * The receiver detector appears to require regular transitions for reliability. This is
 * mentioned in the Linx Application Note AN-00160
 *
 * https://linxtechnologies.com/wp/wp-content/uploads/an-00160.pdfote
 *
 * To help the reciver set its levels, transmission starts with the synchronising character '*'.
 *
 * The ASCII value for '*' is 0b00101010 so that there are several transitions to allow the
 * receiver to get itself sorted out.
 *
 * An alternative sync character might be 'U' (0b01010101) but the '*' seems marginally
 * more effective.
 *
 * Data transmission should begin immediately. For reliable transmission, there should be no
 * delays between successive characters.
 *
 * As soon as the gate is 'broken' - the beam is interrupted - a string is transmitted. The
 * string consists of the synchronising character followed by a sequence of characters that
 * provides both timing and identification. Identification is given by the characters used.
 *
 * The message string contains several items of information and has the general format:
 *
 *   "*GNSSSC#"
 *
 * where
 *
 *  - 'G'    is an ADCII character representing the gate ID
 *           Each gate has an ID in the range 0-15, set with jumpers on pins D6-D9. The ID is read
 *           at system startup and transmitted as an ASCII character in the range 'A' - 'P'
 *           Generally, the gate detector circuit handles two detection circuits although
 *           only one is used except in the start square.
 *           If the second detector is the source of the message, the gate identifier
 *           will be the lower case letter 'a' - 'p'.
 *
 *  - 'N'    is an ASCI digit in the range '0' - '9' representing the sequence number. As
 *           soon as a gate is broken, the first packet is sent. After that, nine more
 *           packets are sent with the same information but incrementing sequence numbers.
 *           The packets are sent at accurate 50ms intervals so that the receiver can
 *           examine a message packet and determine the time at which the gate was actually
 *           broken. The receiver may act upon the first valid packet and ignore subsequent ones
 *           or it may choose to combine packets for reliability.
 *           Sustained interference lasting more than half a second will cause the event to
 *           be missed.
 *           The receiver may register the next event in several ways
 *             - employ a lockout delay so that no packets will be registered for some period
 *             - only repond to another packet if the sequence number is less or equal to the last one
 *             - ignore subsequent packets from the same gate ID.
 *
 *  - 'SSS' is three digits repreenting the numerical value of the gate sensor steady state reading.
 *          This can be used to identify faulty or unreliable gates or potential interference from
 *          ambient illumination.
 *
 *  - 'C'   is a single character check digit as a simple means of error detection. Each of the
 *          preceding characters in the packet is XORed into a byte. That byte is then
 *          reduced to a range of 0-15 and used to generate a character in the range 'a' to 'o'.
 *          Not the most reliable check digit in the world but better than nothing.
 *
 *  - '#'   is a terminating character used as a visual and coding aid when unpacking a
 *          packet.
 *
 *
 * There can be several goal gates in a system since they will not transmit simultaneously
 * and are all equivalent in terms of the timing function in a contest.
 *
 * All gates should have a unique identifier. Currently that limits the number of gates to 16.
 *
 * If multiple contests use the same gate and controller system, there is a small chance that
 * two gates will transmit simultaneously. The probability is low and could be essentially
 * eliminated if the message packets were sent at pseudo random intervals or if a gate
 * uses shared propogation tachniques like those used in ethernet.
 *
 * Once the '*' has been received, the controller records the time and then checks the following
 * character(s) to find out which gate has sent the message. Any of the subsequent characters will
 * serve as positive identifiers.
 *
 */