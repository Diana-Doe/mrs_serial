## Serial protocol

Messages sent/received throuh the serial line consist of 8 bit values.
The protocol for serial communication used at MRS (Baca protocol) is defined as follows:

```
['b'][payload_size][payload_0(=message_id)][payload_1]...[payload_n][checksum]
```

Each character inside [] brackets reperesents one 8 bit value.
the first byte is the character 'b', which represents the start of a message.
Next byte is the payload size. Payload of the message can be 1 to 256 bytes long.
First byte of the payload is message_id, which is user defined and
serves to differentiate between different messages of the same length.
The message_id can be followed by other payload bytes.
The last byte of the message is a checksum, which is calculated as follows:
```
uint8_t checksum = 'b' + payload_size + payload0 + payload1 + ... + payload_n
```
The checksum is calculated by the sender and added to the serial message. The receiver then
calculates the checksum again from the received data, and compares it to the received checksum
value. If they match, the message is considered valid, if they do not match, the message is discarded.

## Reserved messages

Following messages are already reserved for parts of the MRS system, avoid using them:
```
payload_size = 3 && message_id = 0   >> Garmin rangefinder
payload_size = 3 && message_id = 1   >> Garmin rangefinder (up)
payload_size = 1 && message_id = '4'(0x34)   >> Beacon on (eagle)
payload_size = 1 && message_id = '5'(0x35)   >> Beacon off (eagle)
payload_size = 1 && message_id = '7'(0x37)   >> netgun safe (eagle)
payload_size = 1 && message_id = '7'(0x37)   >> netgun arm (eagle)
payload_size = 1 && message_id = '7'(0x37)   >> netgun fire (eagle)
```

## How to use - getting data from a serial device to ROS
If the mrs_serial node is running, and it is connected to some device through the serial line,
it will publish all the messages that are received through the serial line at a topic called
```
/uav_name/mrs_serial/received_message
```
The ros message published by mrs_serial will have this structure (defined in mrs_msgs):
```
time stamp
uint8[] payload
uint8 checksum
bool checksum_correct
```
by default, mrs_serial will only publish messages with correct checksums, other messages will be discraded.

Here is an example of a Arduino function that will send a 16 bit integer through the serial line:
```c
void send_data(uint16_t data) {
  uint8_t checksum = 0;
  uint8_t payload_size = 3;

  byte bytes[2];
  //split 16 bit integer to two 8 bit integers
  bytes[0] = (data >> 8) & 0xFF;
  bytes[1] = data & 0xFF;

  //message start
  Serial.write('b');
  checksum += 'b';

  //payload size
  Serial.write(payload_size);
  checksum += payload_size;

  //payload
  Serial.write(0x17); // message_id
  checksum += 0x17;
  
  Serial.write(bytes[0]);
  checksum += bytes[0];
  
  Serial.write(bytes[1]);
  checksum += bytes[1];
  
  //checksum
  Serial.write(checksum);
}
```



