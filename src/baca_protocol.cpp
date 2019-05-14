#include <ros/package.h>
#include <ros/ros.h>
#include <sensor_msgs/Range.h>
#include <std_msgs/Char.h>
#include <std_srvs/Trigger.h>
#include <std_srvs/SetBool.h>
#include <std_msgs/Bool.h>

#include <string>
#include <mrs_msgs/BacaProtocol.h>

#include "serial_port.h"

#define BUFFER_SIZE 256

// for garmin
#define MAXIMAL_TIME_INTERVAL 1
#define MAX_RANGE 4000  // cm
#define MIN_RANGE 10    // cm


/* class BacaProtocol //{ */

class BacaProtocol {
public:
  BacaProtocol();

  void serialDataCallback(uint8_t data);

  enum serial_receiver_state
  {
    WAITING_FOR_MESSSAGE,
    EXPECTING_SIZE,
    EXPECTING_PAYLOAD,
    EXPECTING_CHECKSUM
  };


  ros::ServiceServer netgun_arm;
  ros::ServiceServer netgun_safe;
  ros::ServiceServer netgun_fire;
  ros::ServiceServer uvled_start_left;
  ros::ServiceServer uvled_start_right;
  ros::ServiceServer uvled_stop;
  ros::ServiceServer board_switch;
  ros::ServiceServer beacon_on;
  ros::ServiceServer beacon_off;

  bool callbackNetgunSafe(std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &res);
  bool callbackNetgunArm(std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &res);
  bool callbackNetgunFire(std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &res);
  bool callbackUvLedStartLeft(std_srvs::SetBool::Request &req, std_srvs::SetBool::Response &res);
  bool callbackUvLedStartRight(std_srvs::SetBool::Request &req, std_srvs::SetBool::Response &res);
  bool callbackUvLedStop(std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &res);
  bool callbackBoardSwitch(std_srvs::SetBool::Request &req, std_srvs::SetBool::Response &res);
  bool callbackBeaconOn(std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &res);
  bool callbackBeaconOff(std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &res);
  void fireTopicCallback(const std_msgs::BoolConstPtr &msg);

  uint8_t connectToSensor(void);
  void    releaseSerialLine(void);
  void    processMessage(uint8_t payload_size, uint8_t *input_buffer, uint8_t checksum, bool checksum_correct);

  ros::Time lastReceived;

  ros::NodeHandle nh_;
  ros::Publisher  range_publisher_;
  ros::Publisher  range_publisher_up_;
  ros::Publisher  baca_protocol_publisher_;

  ros::Subscriber fire_subscriber;

  serial_device::SerialPort *    serial_port_;
  boost::function<void(uint8_t)> serial_data_callback_function_;

  std::string portname_;

  bool enable_servo_;
  bool enable_uvleds_;
  bool enable_switch_;
  bool enable_beacon_;
};

//}

/* BacaProtocol() //{ */

BacaProtocol::BacaProtocol() {

  // Get paramters
  nh_ = ros::NodeHandle("~");

  ros::Time::waitForValid();

  nh_.param("portname", portname_, std::string("/dev/ttyUSB0"));
  nh_.param("enable_servo", enable_servo_, false);
  nh_.param("enable_uvleds", enable_uvleds_, false);
  nh_.param("enable_switch", enable_switch_, false);
  nh_.param("enable_beacon", enable_beacon_, false);

  // Publishers
  range_publisher_         = nh_.advertise<sensor_msgs::Range>("range", 1);
  range_publisher_up_      = nh_.advertise<sensor_msgs::Range>("range_up", 1);
  baca_protocol_publisher_ = nh_.advertise<mrs_msgs::BacaProtocol>("baca_protocol_out", 1);

  fire_subscriber = nh_.subscribe("fire_topic", 1, &BacaProtocol::fireTopicCallback, this, ros::TransportHints().tcpNoDelay());

  // service out

  if (enable_servo_) {
    netgun_arm  = nh_.advertiseService("netgun_arm", &BacaProtocol::callbackNetgunArm, this);
    netgun_safe = nh_.advertiseService("netgun_safe", &BacaProtocol::callbackNetgunSafe, this);
    netgun_fire = nh_.advertiseService("netgun_fire", &BacaProtocol::callbackNetgunFire, this);
  }
  if (enable_uvleds_) {
    uvled_start_left  = nh_.advertiseService("uvled_start_left", &BacaProtocol::callbackUvLedStartLeft, this);
    uvled_start_right = nh_.advertiseService("uvled_start_right", &BacaProtocol::callbackUvLedStartRight, this);
    uvled_stop        = nh_.advertiseService("uvled_stop", &BacaProtocol::callbackUvLedStop, this);
  }
  if (enable_beacon_) {
    beacon_on  = nh_.advertiseService("beacon_start", &BacaProtocol::callbackBeaconOn, this);
    beacon_off = nh_.advertiseService("beacon_stop", &BacaProtocol::callbackBeaconOff, this);
  }
  if (enable_switch_) {
    board_switch = nh_.advertiseService("board_switch", &BacaProtocol::callbackBoardSwitch, this);
  }
  // Output loaded parameters to console for double checking
  ROS_INFO("[%s] is up and running with the following parameters:", ros::this_node::getName().c_str());
  ROS_INFO("[%s] portname: %s", ros::this_node::getName().c_str(), portname_.c_str());
  ROS_INFO("[%s] enable_servo: %d", ros::this_node::getName().c_str(), enable_servo_);
  ROS_INFO("[%s] enable_switch: %d", ros::this_node::getName().c_str(), enable_switch_);
  ROS_INFO("[%s] enable_uvleds: %d", ros::this_node::getName().c_str(), enable_uvleds_);
  lastReceived = ros::Time::now();

  connectToSensor();
}

//}

// | ------------------------ services ------------------------ |

/*  callbackNetgunSafe()//{ */

bool BacaProtocol::callbackNetgunSafe([[maybe_unused]] std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &res) {

  char id      = '7';
  char tmpSend = 'a';
  char crc     = tmpSend;

  serial_port_->sendChar(tmpSend);
  tmpSend = 1;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);
  tmpSend = id;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);
  serial_port_->sendChar(crc);

  ROS_INFO("Safing net gun");
  res.message = "Safing net gun";
  res.success = true;

  return true;
}

//}

/* callbackNetgunArm() //{ */

bool BacaProtocol::callbackNetgunArm([[maybe_unused]] std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &res) {

  char id      = '8';
  char tmpSend = 'a';
  char crc     = tmpSend;

  serial_port_->sendChar(tmpSend);
  tmpSend = 1;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);
  tmpSend = id;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);
  serial_port_->sendChar(crc);

  ROS_INFO("Arming net gun");
  res.message = "Arming net gun";
  res.success = true;

  return true;
}

//}

/* callbackNetgunFire() //{ */

bool BacaProtocol::callbackNetgunFire([[maybe_unused]] std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &res) {

  char id      = '9';
  char tmpSend = 'a';
  char crc     = tmpSend;

  serial_port_->sendChar(tmpSend);
  tmpSend = 1;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);
  tmpSend = id;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);
  serial_port_->sendChar(crc);

  ROS_INFO("Firing net gun");
  res.message = "Firing net gun";
  res.success = true;
  return true;
}

//}

/* callbackBeaconOn() //{ */

bool BacaProtocol::callbackBeaconOn([[maybe_unused]] std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &res) {

  char id      = '4';
  char tmpSend = 'a';
  char crc     = tmpSend;

  serial_port_->sendChar(tmpSend);
  tmpSend = 1;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);
  tmpSend = id;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);
  serial_port_->sendChar(crc);

  ROS_INFO("Starting sirene and beacon");
  res.message = "Starting sirene and beacon";
  res.success = true;
  return true;
}

//}

/* callbackBeaconOff() //{ */

bool BacaProtocol::callbackBeaconOff([[maybe_unused]] std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &res) {

  char id      = '5';
  char tmpSend = 'a';
  char crc     = tmpSend;

  serial_port_->sendChar(tmpSend);
  tmpSend = 1;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);
  tmpSend = id;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);
  serial_port_->sendChar(crc);

  ROS_INFO("Stopping sirene and beacon");
  res.message = "Stopping sirene and beacon";
  res.success = true;
  return true;
}

//}

/* callbackUvLedStartLeft() //{ */

bool BacaProtocol::callbackUvLedStartLeft(std_srvs::SetBool::Request &req, std_srvs::SetBool::Response &res) {

  char id      = '1';
  char tmpSend = 'a';
  char crc     = tmpSend;

  serial_port_->sendChar(tmpSend);

  tmpSend = 2;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);

  tmpSend = id;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);

  tmpSend = (uint8_t)req.data;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);

  serial_port_->sendChar(crc);

  ROS_INFO("Starting left UV leds. f: %d", req.data);
  res.message = "Starting left UV leds";
  res.success = true;
  return true;
}

//}

/* callbackUvLedStartRight() //{ */

bool BacaProtocol::callbackUvLedStartRight(std_srvs::SetBool::Request &req, std_srvs::SetBool::Response &res) {

  char id      = '2';
  char tmpSend = 'a';
  char crc     = tmpSend;

  serial_port_->sendChar(tmpSend);

  tmpSend = 2;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);

  tmpSend = id;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);

  tmpSend = (uint8_t)req.data;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);

  serial_port_->sendChar(crc);

  ROS_INFO("Starting right UV leds. f: %d", req.data);
  res.message = "Starting right UV leds";
  res.success = true;

  return true;
}

//}

/* callbackUvLedStop() //{ */

bool BacaProtocol::callbackUvLedStop([[maybe_unused]] std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &res) {

  char id      = '3';
  char tmpSend = 'a';
  char crc     = tmpSend;

  serial_port_->sendChar(tmpSend);
  tmpSend = 1;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);
  tmpSend = id;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);
  serial_port_->sendChar(crc);

  ROS_INFO("Stopping UV LEDs");
  res.message = "Stopping UV LEDs";
  res.success = true;

  return true;
}

//}

/* callbackBoardSwitch() //{ */

bool BacaProtocol::callbackBoardSwitch(std_srvs::SetBool::Request &req, std_srvs::SetBool::Response &res) {

  char id      = '4';
  char tmpSend = 'a';
  char crc     = tmpSend;

  serial_port_->sendChar(tmpSend);

  tmpSend = 2;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);

  tmpSend = id;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);

  tmpSend = (uint8_t)req.data;
  crc += tmpSend;
  serial_port_->sendChar(tmpSend);

  serial_port_->sendChar(crc);

  ROS_INFO("Switching output to: %d", req.data);
  res.message = "Output switched";
  res.success = true;

  return true;
}

//}

/* fireTopicCallback //{ */

void BacaProtocol::fireTopicCallback(const std_msgs::BoolConstPtr &msg) {
  std_msgs::Bool mybool = *msg;
  if (mybool.data) {
    char id      = '9';
    char tmpSend = 'a';
    char crc     = tmpSend;

    serial_port_->sendChar(tmpSend);
    tmpSend = 1;
    crc += tmpSend;
    serial_port_->sendChar(tmpSend);
    tmpSend = id;
    crc += tmpSend;
    serial_port_->sendChar(tmpSend);
    serial_port_->sendChar(crc);

    ROS_INFO("Firing net gun");
  }
}

//}

// |                          routines                          |

/* connectToSensors() //{ */

uint8_t BacaProtocol::connectToSensor(void) {

  // Create serial port
  serial_port_ = new serial_device::SerialPort();

  // Set callback function for the serial ports
  serial_data_callback_function_ = boost::bind(&BacaProtocol::serialDataCallback, this, _1);
  serial_port_->setSerialCallbackFunction(&serial_data_callback_function_);

  // Connect serial port
  ROS_INFO("[%s]: Openning the serial port.", ros::this_node::getName().c_str());
  if (!serial_port_->connect(portname_)) {
    ROS_ERROR("[%s]: Could not connect to sensor.", ros::this_node::getName().c_str());
    return 0;
  }

  ROS_INFO("[%s]: Connected to sensor.", ros::this_node::getName().c_str());

  lastReceived = ros::Time::now();

  return 1;
}

//}

/* releaseSerialLine() //{ */

void BacaProtocol::releaseSerialLine(void) {

  delete serial_port_;
}

//}

/* serialDataCallback() //{ */

void BacaProtocol::serialDataCallback(uint8_t single_character) {

  static serial_receiver_state rec_state    = WAITING_FOR_MESSSAGE;
  static uint8_t               payload_size = 0;
  static uint8_t               input_buffer[BUFFER_SIZE];
  static uint8_t               buffer_counter = 0;
  static uint8_t               checksum       = 0;

  switch (rec_state) {
    case WAITING_FOR_MESSSAGE:

      if (single_character == 'b' ||
          single_character == 'a') {  // the 'a' is there for backwards-compatibility, going forwards all messages should start with 'b'
        checksum       = single_character;
        buffer_counter = 0;
        rec_state      = EXPECTING_SIZE;
      }
      break;

    case EXPECTING_SIZE:

      if (single_character == 0) {
        ROS_ERROR("[%s]: Message with 0 payload_size received, discarding.", ros::this_node::getName().c_str());
        rec_state = WAITING_FOR_MESSSAGE;
      } else {
        payload_size = single_character;
        checksum += single_character;
        rec_state = EXPECTING_PAYLOAD;
      }
      break;

    case EXPECTING_PAYLOAD:

      input_buffer[buffer_counter] = single_character;
      checksum += single_character;
      buffer_counter++;
      if (buffer_counter >= payload_size) {
        rec_state = EXPECTING_CHECKSUM;
      }
      break;

    case EXPECTING_CHECKSUM:

      if (checksum == single_character) {
        processMessage(payload_size, input_buffer, checksum, true);
        lastReceived = ros::Time::now();
        rec_state    = WAITING_FOR_MESSSAGE;
      } else {
        ROS_ERROR_STREAM("[ " << ros::this_node::getName().c_str() << "]: Message with bad crc received, received: " << static_cast<unsigned>(single_character)
                              << ", calculated: " << static_cast<unsigned>(checksum));
        rec_state = WAITING_FOR_MESSSAGE;
      }
      break;
  }
}

//}

/* processMessage() //{ */

void BacaProtocol::processMessage(uint8_t payload_size, uint8_t *input_buffer, uint8_t checksum, bool checksum_correct) {

  if (payload_size == 3 && (input_buffer[0] == 0x00 || input_buffer[0] == 0x01) && checksum_correct) {
    /* Special message reserved for garmin rangefinder */
    uint8_t message_id = input_buffer[0];
    int16_t range      = input_buffer[1] << 8;
    range |= input_buffer[2];

    sensor_msgs::Range range_msg;
    range_msg.field_of_view  = 0.0523599;  // +-3 degree
    range_msg.max_range      = MAX_RANGE * 0.01;
    range_msg.min_range      = MIN_RANGE * 0.01;
    range_msg.radiation_type = sensor_msgs::Range::INFRARED;
    range_msg.header.stamp   = ros::Time::now();

    range_msg.range = range * 0.01;  // convert to m

    if (range > MAX_RANGE) {
      range_msg.range = std::numeric_limits<double>::infinity();
    } else if (range < MIN_RANGE) {
      range_msg.range = -std::numeric_limits<double>::infinity();
    }

    if (message_id == 0x00) {
      range_msg.header.frame_id = "garmin_frame";
      range_publisher_.publish(range_msg);
    } else if (message_id == 0x01) {
      range_msg.header.frame_id = "garmin_frame_up";
      range_publisher_up_.publish(range_msg);
    }
  } else {
    /* General serial message */
    mrs_msgs::BacaProtocol msg;
    msg.stamp = ros::Time::now();
    for (uint8_t i = 0; i < payload_size; i++) {
      msg.payload.push_back(input_buffer[i]);
    }
    msg.checksum         = checksum;
    msg.checksum_correct = checksum_correct;
    ROS_INFO("[%s]: published", ros::this_node::getName().c_str());
    baca_protocol_publisher_.publish(msg);
  }
}

//}

/* main() //{ */

int main(int argc, char **argv) {

  ros::init(argc, argv, "BacaProtocol");

  BacaProtocol serial_line;

  ros::Rate loop_rate(100);

  while (ros::ok()) {

    // check whether the teraranger stopped sending data
    ros::Duration interval = ros::Time::now() - serial_line.lastReceived;
    if (interval.toSec() > MAXIMAL_TIME_INTERVAL) {

      serial_line.releaseSerialLine();

      ROS_WARN("[%s]: BacaProtocol not responding, resetting connection...", ros::this_node::getName().c_str());

      // if establishing the new connection was successfull
      if (serial_line.connectToSensor() == 1) {

        ROS_INFO("[%s]: New connection to BacaProtocol was established.", ros::this_node::getName().c_str());
      }
    }
    ros::spinOnce();
    loop_rate.sleep();
  }

  return 0;
}

//}
