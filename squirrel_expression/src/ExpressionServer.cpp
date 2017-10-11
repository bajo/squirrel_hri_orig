/**
 * Expression server.
 * Takes expression messages (essentially strings like "SURPRISED") and issues
 * the corresponding lower level behaviours: sounds, faces, base movements.
 * 
 * authors: Federico Boniardi, Michael Zillich
 * date: Feb 2016
 */

#include <sstream>
#include <ros/ros.h>
#include <sound_play/SoundRequest.h>
#include <squirrel_expression/ExpressionServer.h>
#include <squirrel_hri_msgs/Expression.h>
#include <squirrel_interaction/DisplayScreen.h>

namespace SQUIRREL_expression
{
  ExpressionServer::ExpressionServer(ros::NodeHandle &nh)
  {
    if(!nh.getParam("/squirrel_expression/sound_directory", sound_dir))
    {
      ROS_ERROR("You have to specify 'sound_directory', where all sound samples are stored.");
      exit(1);
    }

    sound_files[squirrel_hri_msgs::Expression::NEUTRAL] = "";
    sound_files[squirrel_hri_msgs::Expression::HELLO] = "greeting_in-01.wav";
    sound_files[squirrel_hri_msgs::Expression::GOODBYE] = "greeting_out-01.wav";
    sound_files[squirrel_hri_msgs::Expression::OK] = "Ok_01.wav";
    sound_files[squirrel_hri_msgs::Expression::YES] = "Yes_02.wav";
    sound_files[squirrel_hri_msgs::Expression::NO] = "no+sound-01.wav";
    sound_files[squirrel_hri_msgs::Expression::CHEERING] = "cheering_positive-01.wav";
    sound_files[squirrel_hri_msgs::Expression::OH_NO] = "extra_start_01.wav";
    sound_files[squirrel_hri_msgs::Expression::CONFUSED] = "confused-06.wav";
    sound_files[squirrel_hri_msgs::Expression::SURPRISED] = "cheering_positive-02.wav";
    sound_files[squirrel_hri_msgs::Expression::ASKING] = "confused-03.wav";
    sound_files[squirrel_hri_msgs::Expression::THINKING] = "confused-04.wav";
    sound_files[squirrel_hri_msgs::Expression::HERE_HERE] = "Here-01.wav";
    sound_files[squirrel_hri_msgs::Expression::OUCH] = "confused-01-uhoh.wav";
    // for these expressions there is no associated sound
    sound_files[squirrel_hri_msgs::Expression::ANTAGONIST] = "";
    sound_files[squirrel_hri_msgs::Expression::REFERENTIAL_INFO] = "";
    sound_files[squirrel_hri_msgs::Expression::HANDOVER_OBJECT_DOWN] = "";
    sound_files[squirrel_hri_msgs::Expression::HAND_TO_HAND] = "";
    sound_files[squirrel_hri_msgs::Expression::PUSHING] = "";

    // faces are currently:
    // blank cheerful confused look_down look_front look_left look_right no think
    faces[squirrel_hri_msgs::Expression::NEUTRAL] = "look_front";
    faces[squirrel_hri_msgs::Expression::HELLO] = "look_front";
    faces[squirrel_hri_msgs::Expression::GOODBYE] = "cheerful";
    faces[squirrel_hri_msgs::Expression::OK] = "cheerful";
    faces[squirrel_hri_msgs::Expression::YES] = "cheerful";
    faces[squirrel_hri_msgs::Expression::NO] = "no";
    faces[squirrel_hri_msgs::Expression::CHEERING] = "cheerful";
    faces[squirrel_hri_msgs::Expression::OH_NO] = "no";
    faces[squirrel_hri_msgs::Expression::CONFUSED] = "confused";
    faces[squirrel_hri_msgs::Expression::SURPRISED] = "think";
    faces[squirrel_hri_msgs::Expression::ASKING] = "confused";
    faces[squirrel_hri_msgs::Expression::THINKING] = "think";
    faces[squirrel_hri_msgs::Expression::HERE_HERE] = "cheerful";
    faces[squirrel_hri_msgs::Expression::OUCH] = "no";
    // for these expressions there is no associated face
    faces[squirrel_hri_msgs::Expression::ANTAGONIST] = "";
    faces[squirrel_hri_msgs::Expression::REFERENTIAL_INFO] = "";
    faces[squirrel_hri_msgs::Expression::HANDOVER_OBJECT_DOWN] = "";
    faces[squirrel_hri_msgs::Expression::HAND_TO_HAND] = "";
    faces[squirrel_hri_msgs::Expression::PUSHING] = "";

    // HACK: sound play has problems on the Robotino, so just directly use aplay
    // This should be fixed at some point.
    sound_pub = nh.advertise<sound_play::SoundRequest>("/robotsound", 10, true);

    face_pub = nh.advertise<std_msgs::String>("/face/emotion", 10, true);
    face_client = nh.serviceClient<squirrel_interaction::DisplayScreen>("/face/emotion");
  }

  void ExpressionServer::performExpression(const std_msgs::String::ConstPtr& msg)
  {
    ROS_INFO("Expressions: make expression '%s'", msg->data.c_str());
    performFace(msg->data);
    // HACK: this is only needed to synchrniace face and sound on the IDM robot
    // usleep(600000);
    performSound(msg->data);
  }

  void ExpressionServer::performSound(const std::string &expression)
  {
    std::string filename = sound_files[expression];
    if(!filename.empty())
    {
      std::stringstream path;
      path << sound_dir << "/" << filename;
      sound_play::SoundRequest sound_msg;
      sound_msg.sound = -2; // play file
      sound_msg.command = 1; // play once
      sound_msg.arg = path.str();
      sound_pub.publish(sound_msg);
      // HACK: sound play has problems on the Robotino, so just directly use aplay
      // This should be fixed at some point.
      std::stringstream cmd;
      cmd << "aplay -D sysdefault " << path.str();
      std::cout << cmd.str() << std::endl;
      system(cmd.str().c_str());
      std::cout << "aplay did finish" << std::endl;
    }
    else
    {
      ROS_ERROR("unknown sound expression '%s'", expression.c_str());
    }
  }

  void ExpressionServer::performFace(const std::string &expression)
  {
    std::string face_msg = faces[expression];
    std_msgs::String msg; 
    msg.data = face_msg;
    ROS_INFO("Expressions: make facial expression '%s'",face_msg.c_str());
    face_pub.publish(msg);
    if(!face_msg.empty())
    {
      squirrel_interaction::DisplayScreen srv;
      srv.request.message = face_msg;
      if(!face_client.call(srv))
        ROS_ERROR("Failed to call display_screen service");
    }
    else
    {
      ROS_ERROR("unknown face expression '%s'", expression.c_str());
    }
  }

  /**
   * NOTE: for now this simply nods with the pan tilt. Probably this
   * will go.
   */
  void ExpressionServer::performHead(const std::string &expression)
  {
    // nod the head
    ros::Rate nodRate(2.5);
    std_msgs::Float64 ht;
    for(int i=0;i<3;i++)
    {
      ht.data = 0.5;
      head_tilt_pub.publish(ht);
      nodRate.sleep();
      ht.data = 0.0;
      head_tilt_pub.publish(ht);
      nodRate.sleep();
    }
    nodRate.sleep();
    ht.data = 0.6;
    head_tilt_pub.publish(ht);
  }

} // close namespace

/*-------------*/
/* Main method */
/*-------------*/

int main(int argc, char **argv)
{
  ros::init(argc, argv, "squirrel_expression");
  ros::NodeHandle nh;

  // listen for expression commands
  SQUIRREL_expression::ExpressionServer es(nh);
  ros::Subscriber expressionSub = nh.subscribe("/expression", 1, &SQUIRREL_expression::ExpressionServer::performExpression, &es);

  ROS_INFO("Expressions: Ready to receive");

  ros::spin();
  return 0;
}
