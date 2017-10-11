/**
 * Expression server.
 * Takes expression messages (essentially strings like "SURPRISED") and issues
 * the corresponding lower level behaviours: sounds, faces, base movements.
 * 
 * authors: Federico Boniardi, Michael Zillich
 * date: Feb 2016
 */

#include <string>
#include <map>
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <std_msgs/Float64.h>

#ifndef SQUIRREL_expression
#define SQUIRREL_expression

/**
 * This file defines the ExpressionServer class.
 * The expression server listens for expression commands,
 * and makes the robotino dance.
 */
namespace SQUIRREL_expression
{

class ExpressionServer
{
private:
  ros::Publisher sound_pub;
  ros::Publisher face_pub;
  ros::Publisher head_tilt_pub;
  ros::ServiceClient face_client;

  std::string sound_dir;
  std::map<std::string, std::string> sound_files;
  std::map<std::string, std::string> faces;

  void performSound(const std::string &expression);
  void performFace(const std::string &expression);
  void performHead(const std::string &expression);

public:
  ExpressionServer(ros::NodeHandle &nh);

  void performExpression(const std_msgs::String::ConstPtr& msg);
};

}
#endif
