#include "squirrel_people_tracker/follow_child.h"
#include <actionlib/client/simple_action_client.h>
#include <string>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <tf/tf.h>
#include <actionlib/client/terminal_state.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_types.h>
#include <pcl/filters/passthrough.h>
#include <pcl/common/common.h>
#include "pcl_ros/point_cloud.h"
#include <pcl/io/pcd_io.h>

#include "squirrel_view_controller_msgs/LookAtPosition.h"


typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;
typedef pcl::PointXYZRGB PointT;

ChildFollowingAction::~ChildFollowingAction(void)
{
  if (move_base_ac_)
    delete move_base_ac_;
}

ChildFollowingAction::ChildFollowingAction(std::string name) : as_(nh_, name, false), action_name_(name)
{
  init_ = ros::Time::now();
  id_ = 0;
  
  pan_tilt_client_ = nh_.serviceClient<squirrel_view_controller_msgs::LookAtPosition>("/squirrel_view_controller/look_at_position", true);
  if (!(ros::service::waitForService(pan_tilt_client_.getService(), ros::Duration(5.0))))
  {
    ROS_ERROR("wait for service %s failed", pan_tilt_client_.getService().c_str());
    return;
  }

  move_base_ac_ = new MoveBaseClient("move_base", true);
  if (!move_base_ac_->waitForServer(ros::Duration(15.0)))
  {
    ROS_ERROR("Waiting for the move_base action server to come up");
    return;
  }
  distance_ = 0.8;
  // register the goal and feeback callbacks
  as_.registerGoalCallback(boost::bind(&ChildFollowingAction::goalCB, this));
  as_.registerPreemptCallback(boost::bind(&ChildFollowingAction::preemptCB, this));

  // subscribe to the data topic of interest
  sub_ = nh_.subscribe("people_tracker_measurements", 1, &ChildFollowingAction::analysisCB, this);
  as_.start();

  // publishers
  pub_ = nh_.advertise<geometry_msgs::PoseStamped>("published_topic", 1);
  vis_pub_ = nh_.advertise<visualization_msgs::Marker>( "visualization_marker", 0 );
  cloud_pub_ = nh_.advertise<pcl::PointCloud<PointT> > ("filtered_cloud", 5, true);
  ROS_INFO("Ready to accept goals...");
}

void ChildFollowingAction::goalCB()
{
  goal_ = as_.acceptNewGoal();
  for (size_t i=0; i < goal_->target_locations.size(); ++i)
  {
    ROS_INFO("publish target location markers.");
    publishGoalMarker(goal_->target_locations[i].x, goal_->target_locations[i].y, 0.0, 1.0, 0.0, 0.0, "child_target_locations");
    id_ += 1;
  }
}

void ChildFollowingAction::preemptCB()
{
  ROS_INFO("%s: Preempted", action_name_.c_str());
  // set the action state to preempted
  as_.setPreempted();
}

void ChildFollowingAction::analysisCB(const people_msgs::PositionMeasurementArray::ConstPtr &msg)
{
  ROS_INFO("Data received. processing ...");
  // make sure that the action hasn't been canceled
  if (!as_.isActive())
  {
    ROS_DEBUG("Action server %s is no longer active. Exiting.", action_name_.c_str());
    return;
  }
  if (msg->people.size() == 0)
  {
    ROS_DEBUG("No people in message"); 
    return;
  }
  actionlib::SimpleClientGoalState state = move_base_ac_->getState();
  ROS_DEBUG("Action in state: %s",state.toString().c_str());
  

  geometry_msgs::PoseStamped robot_pose, child_pose, tmp_pose, out_pose, verify_pose;
  move_base_msgs::MoveBaseGoal move_base_goal_;
  
  double min_distance = 1000.0;
  int index = 0;
  double height = 0.0;
  double time_diff = (ros::Time::now() - init_).toSec();

  ROS_DEBUG("time diff: %f", time_diff);
  if (time_diff < 1.0)
  {
    return;
  }

  bool child_present = false;
  // calculate distance to select the closest personCB
  ROS_INFO("%ld people received in message",msg->people.size());
  for (size_t i = 0; i < msg->people.size(); ++i)
  {
    double distance = (sqrt(msg->people[i].pos.x*msg->people[i].pos.x + msg->people[i].pos.y*msg->people[i].pos.y));

    tmp_pose.header.stamp = ros::Time(0);
    tmp_pose.header.frame_id = "hokuyo_link";
    tmp_pose.pose.position.x = msg->people[i].pos.x;
    tmp_pose.pose.position.y = msg->people[i].pos.y;
    tmp_pose.pose.orientation =  tf::createQuaternionMsgFromYaw(0.0);

    if (distance < min_distance)
    {
      LookAtChild(&tmp_pose);
      index = i;
      min_distance = distance;
      verify_pose = tmp_pose;
    }
  }
  child_present = VerifyChildAtPose(&verify_pose, height);
  LookAtChild(&verify_pose, height);

  // we did not detect a child
  if (index == 0 && !child_present)
    return;

  double alpha = 0.0;
  tmp_pose.header.stamp = ros::Time(0);
  tmp_pose.header.frame_id = "hokuyo_link";
  tmp_pose.pose.position.x = msg->people[index].pos.x;
  tmp_pose.pose.position.y = msg->people[index].pos.y;
  tmp_pose.pose.orientation =  tf::createQuaternionMsgFromYaw(alpha);
  try{
    ros::Time now = ros::Time(0);
    tfl_.waitForTransform("hokuyo_link", "map", now, ros::Duration(3.0));
    tfl_.transformPose("map", tmp_pose, child_pose);
  }
  catch (tf::TransformException ex){
    ROS_ERROR("%s",ex.what());
    ros::Duration(1.0).sleep();
    return;
  }
  ROS_DEBUG("Person detected at (x, y): (%f, %f) hokuyo_link", tmp_pose.pose.position.x, tmp_pose.pose.position.y);
  ROS_INFO("Person detected at (x, y): (%f, %f) map", child_pose.pose.position.x, child_pose.pose.position.y);

  // check if the child is in one of the target areas
  for (size_t i=0; i < goal_->target_locations.size(); ++i)
  {
    if ((fabs(child_pose.pose.position.x - goal_->target_locations[i].x) < 0.5) &&
        (fabs(child_pose.pose.position.y - goal_->target_locations[i].y) < 0.5))
    {
      // make sure we stop now
      ROS_INFO("%s: Succeeded", action_name_.c_str());
      result_.final_location = child_pose;
      as_.setSucceeded(result_);
    }
  }
  
  // calculate a point between the child and the robot
  alpha = atan2(tmp_pose.pose.position.y, tmp_pose.pose.position.x);
  double k = sqrt(tmp_pose.pose.position.x * tmp_pose.pose.position.x + tmp_pose.pose.position.y * tmp_pose.pose.position.y);
  ROS_DEBUG("k: %f, alpha: %f", k, alpha);
  ROS_DEBUG("sin(alpha): %f, cos(alpha): %f", sin(alpha), cos(alpha));
  
  tmp_pose.header.stamp = ros::Time(0);
  tmp_pose.header.frame_id = "hokuyo_link";
  tmp_pose.pose.position.x = (k - distance_)*cos(alpha);
  tmp_pose.pose.position.y = (k - distance_)*sin(alpha);
  tmp_pose.pose.orientation = tf::createQuaternionMsgFromYaw(alpha);
  
  pub_.publish(tmp_pose);
  try{
    ros::Time now = ros::Time(0);
    tfl_.waitForTransform("hokuyo_link", "map",
                            now, ros::Duration(3.0));
    tfl_.transformPose("map", tmp_pose, out_pose);
  }
  catch (tf::TransformException ex){
    ROS_ERROR("%s",ex.what());
    ros::Duration(1.0).sleep();
    return;
  }
  if (fabs(last_goal_.position.x - out_pose.pose.position.x) < 0.25 && 
      fabs(last_goal_.position.y - out_pose.pose.position.y) < 0.25 &&
      fabs(tf::getYaw(last_goal_.orientation) - tf::getYaw(out_pose.pose.orientation) < 0.26)) //~15 degree
  {
  ROS_DEBUG("Last goal was (x, y): (%f, %f) map", last_goal_.position.x, last_goal_.position.y);
  ROS_DEBUG("Current nav goal would be (x, y): (%f, %f) map", out_pose.pose.position.x, out_pose.pose.position.y);
    ROS_DEBUG("current goal and last goal are close to each other. Do not send new goal");
    return;
  }

  publishGoalMarker(out_pose.pose.position.x, out_pose.pose.position.y, out_pose.pose.position.z, 0.0, 1.0, 0.0, "child_goal");
  ROS_DEBUG("Setting nav goal to (x, y): (%f, %f) hokuyo_link", tmp_pose.pose.position.x, tmp_pose.pose.position.y);
  ROS_INFO("Setting nav goal to (x, y): (%f, %f) map", out_pose.pose.position.x, out_pose.pose.position.y);

  last_goal_.position = out_pose.pose.position;
  last_goal_.orientation = out_pose.pose.orientation;

  // set move_base goal
  move_base_goal_.target_pose.header.frame_id = out_pose.header.frame_id;
  move_base_goal_.target_pose.header.stamp = out_pose.header.stamp;
  move_base_goal_.target_pose.pose.position.x = out_pose.pose.position.x;
  move_base_goal_.target_pose.pose.position.y = out_pose.pose.position.y;
  move_base_goal_.target_pose.pose.orientation = out_pose.pose.orientation;

  ROS_INFO("Sending goal to move_base");
  move_base_ac_->sendGoal(move_base_goal_);
  LookAtChild(&child_pose);

  init_ = ros::Time::now();
  //ros::Duration(0.1).sleep();
}

void ChildFollowingAction::publishGoalMarker(float x, float y, float z, float red, float green, float blue, const char* name)
{
  visualization_msgs::Marker marker;
  marker.header.frame_id = "map";
  marker.header.stamp = ros::Time(0);
  marker.ns = name;
  marker.id = id_;
  marker.type = visualization_msgs::Marker::SPHERE;
  marker.action = visualization_msgs::Marker::ADD;
  marker.pose.position.x = x;
  marker.pose.position.y = y;
  marker.pose.position.z = z;
  marker.pose.orientation.x = 0.0;
  marker.pose.orientation.y = 0.0;
  marker.pose.orientation.z = 0.0;
  marker.pose.orientation.w = 1.0;
  marker.scale.x = 0.2;
  marker.scale.y = 0.2;
  marker.scale.z = 0.2;
  marker.color.a = 1.0; // Don't forget to set the alpha!
  marker.color.r = red;
  marker.color.g = green;
  marker.color.b = blue; 
  vis_pub_.publish(marker);
  ros::Duration(0.01).sleep();
}

bool ChildFollowingAction::VerifyChildAtPose(geometry_msgs::PoseStamped* pose, double &height, double margin)
{
  sensor_msgs::PointCloud2ConstPtr sceneConst;
  sensor_msgs::PointCloud2 scene;
  geometry_msgs::PointStamped point, point_max;
  geometry_msgs::PoseStamped out_pose;
  pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>);
  pcl::PointCloud<PointT>::Ptr outputCloud(new pcl::PointCloud<PointT>);

  // get data from depth camera
  sceneConst =
      ros::topic::waitForMessage<sensor_msgs::PointCloud2>("/kinect/depth_registered/points", nh_, ros::Duration(20));

  // cut away area that is not close to the child candidate's location
  if (sceneConst != NULL)
  {
    scene = *sceneConst;
    pcl::fromROSMsg(scene, *cloud);
    ROS_INFO("cloud frame is %s", scene.header.frame_id.c_str());

    try
    {
      ros::Time now = ros::Time(0);
      listener_.waitForTransform(scene.header.frame_id, pose->header.frame_id, now, ros::Duration(3.0));
      listener_.transformPose(scene.header.frame_id, now, *pose, pose->header.frame_id, out_pose);
    }
    catch (tf::TransformException ex)
    {
      ROS_ERROR("%s", ex.what());
      ros::Duration(1.0).sleep();
      return false;
    }

    visualization_msgs::Marker marker;
    marker.header.frame_id = out_pose.header.frame_id;
    marker.header.stamp = ros::Time(0);
    marker.ns = "cutoff";
    marker.id = 1;
    marker.type = visualization_msgs::Marker::SPHERE;
    marker.action = visualization_msgs::Marker::ADD;
    marker.pose.position.x = out_pose.pose.position.x;
    marker.pose.position.y = out_pose.pose.position.y;
    marker.pose.position.z = out_pose.pose.position.z;
    marker.pose.orientation.x = 0.0;
    marker.pose.orientation.y = 0.0;
    marker.pose.orientation.z = 0.0;
    marker.pose.orientation.w = 1.0;
    marker.scale.x = 0.2;
    marker.scale.y = 0.2;
    marker.scale.z = 0.2;
    marker.color.a = 1.0; // Don't forget to set the alpha!
    marker.color.r = 1.0;
    marker.color.g = 0.0;
    marker.color.b = 0.0;
    vis_pub_.publish(marker);
    ros::Duration(0.01).sleep();

    pcl::PassThrough<PointT> pass;
    pass.setKeepOrganized(true);
    pass.setFilterFieldName("z");
    ROS_INFO("cutting at %s frame : %f", pose->header.frame_id.c_str(), pose->pose.position.x);
    ROS_INFO("cutting Z at %s frame : %f",out_pose.header.frame_id.c_str(), out_pose.pose.position.z);
    pass.setFilterLimits(out_pose.pose.position.z - margin, out_pose.pose.position.z + margin);
    pass.setInputCloud(cloud);
    pass.filter(*cloud);
    pass.setFilterFieldName("x");
    ROS_INFO("cutting X at %s frame : %f",out_pose.header.frame_id.c_str(), out_pose.pose.position.x);
    pass.setFilterLimits(out_pose.pose.position.x - margin, out_pose.pose.position.x + margin);
    pass.setInputCloud(cloud);
    pass.filter(*cloud);
    std::vector<int> indices;
    pcl::removeNaNFromPointCloud(*cloud, *outputCloud, indices);
    cloud_pub_.publish(outputCloud);
    PointT min_p, max_p;
    pcl::getMinMax3D(*outputCloud, min_p, max_p);

    ROS_INFO("min y: %f, max y: %f", min_p.y, max_p.y);

    point.header.frame_id = out_pose.header.frame_id;
    point.header.stamp = out_pose.header.stamp;
    point.point.x = out_pose.pose.position.x;
    point.point.y = min_p.y;
    point.point.z = out_pose.pose.position.z;

    marker.header.frame_id = scene.header.frame_id;
    marker.header.stamp = ros::Time(0);
    marker.ns = "cutoff";
    marker.id = 2;
    marker.type = visualization_msgs::Marker::SPHERE;
    marker.action = visualization_msgs::Marker::ADD;
    marker.pose.position.x = point.point.x;
    marker.pose.position.y = point.point.y;
    marker.pose.position.z = point.point.z;
    marker.pose.orientation.x = 0.0;
    marker.pose.orientation.y = 0.0;
    marker.pose.orientation.z = 0.0;
    marker.pose.orientation.w = 1.0;
    marker.scale.x = 0.2;
    marker.scale.y = 0.2;
    marker.scale.z = 0.2;
    marker.color.a = 1.0; // Don't forget to set the alpha!
    marker.color.r = 0.0;
    marker.color.g = 0.0;
    marker.color.b = 1.0;
    vis_pub_.publish(marker);
    ros::Duration(0.01).sleep();

    try
    {
      ros::Time now = ros::Time(0);
      listener_.waitForTransform("/map", point.header.frame_id, now, ros::Duration(3.0));
      listener_.transformPoint("/map", now, point, point.header.frame_id, point_max);
    }
    catch (tf::TransformException ex)
    {
      ROS_ERROR("%s", ex.what());
      ros::Duration(1.0).sleep();
      return false;
    }
    ROS_INFO("Highest point is at: x: %f, y: %f, z: %f in frame: %s", point.point.x, point.point.y, point.point.z, point.header.frame_id.c_str());
    ROS_INFO("Highest point is at: x: %f, y: %f, z: %f in frame: %s", point_max.point.x, point_max.point.y, point_max.point.z, point_max.header.frame_id.c_str());

    ROS_INFO("Highest point: %f", point_max.point.z);
    // plausibility check
    if ((point_max.point.z < 1.0) || (point_max.point.z > 2.0))
    {
      // height check failed
      ROS_INFO("Highest point is lower than 1.0 or above 2.0 meters. Unlikely to be a standing child.");
      return false;
    }
    height = point_max.point.z;
    ROS_INFO("Height check success. Most likely a child.");
    return true;
  }
  return false;
}

void ChildFollowingAction::LookAtChild(geometry_msgs::PoseStamped* pose, double height)
{
  squirrel_view_controller_msgs::LookAtPosition srv;
  // set pan / tilt goal
  srv.request.target.header.frame_id = pose->header.frame_id;
  srv.request.target.header.stamp = pose->header.stamp;
  srv.request.target.pose.position.x = pose->pose.position.x;
  srv.request.target.pose.position.y = pose->pose.position.y;
  srv.request.target.pose.position.z = height;
  srv.request.target.pose.orientation = tf::createQuaternionMsgFromYaw(0.0);

  if (pan_tilt_client_.call(srv))
  {
    ROS_DEBUG("%s: Reached position", pan_tilt_client_.getService().c_str());
  }
  else
  {
    ROS_ERROR("Failed to call service %s", pan_tilt_client_.getService().c_str());
  }
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "squirrel_child_follower");
  ChildFollowingAction follow_child(ros::this_node::getName());
  ros::spin();
  return 0;
}
