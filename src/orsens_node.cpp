#include <ros/ros.h>
#include <ros/package.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <image_geometry/stereo_camera_model.h>
#include <image_transport/image_transport.h>
#include <sensor_msgs/Image.h>
#include <stereo_msgs/DisparityImage.h>
#include <std_msgs/Int32.h>
#include <sensor_msgs/fill_image.h>
#include <camera_info_manager/camera_info_manager.h>
#include <sstream>


#include <pcl_conversions/pcl_conversions.h> //!

#include <signal.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../include/orsens.h"

using namespace cv;
using namespace sensor_msgs;

ros::Publisher pub_left;
ros::Publisher pub_right;
ros::Publisher pub_disp;
ros::Publisher pub_depth;
ros::Publisher pub_disp_filtered;
ros::Publisher pub_info;
ros::Publisher pub_cloud;

Orsens orsens;

bool working = false;

void sigint_handler(int sig)
{
    working = false;
    orsens.stop();
    ros::shutdown();
}


int main (int argc, char** argv)
{
    // Initialize ROS
    ros::init (argc, argv, "orsens_node");
    ros::NodeHandle nh;

    string capture_mode_string;
    string data_path;
    int color_width, depth_width;
    bool compress_color, compress_depth;
    bool publish_color, publish_disp, publish_depth;

    nh.param<string>("/orsens/capture_mode", capture_mode_string, "depth_only");
    nh.param<string>("/orsens/data_path", data_path, "../data");
    nh.param<int>("/orsens/color_width", color_width, 640);
    nh.param<int>("/orsens/depth_width", depth_width, 640);
    nh.param<bool>("/orsens/compress_color", compress_color, false);
    nh.param<bool>("/orsens/compress_depth", compress_depth, false);
    nh.param<bool>("/orsens/publish_color", publish_color, true);
    nh.param<bool>("/orsens/publish_disp", publish_disp, true);
    nh.param<bool>("/orsens/publish_depth", publish_depth, true);

    Orsens::CaptureMode capture_mode = Orsens::captureModeFromString(capture_mode_string);

    printf("params: %d %s\n", color_width, capture_mode_string.c_str());

    if (!orsens.start(capture_mode, data_path, color_width, depth_width, compress_color, compress_depth))
    {
        ROS_ERROR("unable to start OrSens device, check connection\n");
        return -1;
    }

    // Create a ROS publishers for the output messages
    pub_left = nh.advertise<sensor_msgs::Image> ("/orsens/left", 1);
    pub_right = nh.advertise<sensor_msgs::Image> ("/orsens/right", 1);
    pub_disp = nh.advertise<sensor_msgs::Image> ("/orsens/disparity", 1); // 0-255
    pub_depth = nh.advertise<sensor_msgs::Image> ("/orsens/depth", 1); // uint16 in mm
//    pub_info = nh.advertise<sensor_msgs::CameraInfo>("/orsens/camera_info", 1);
//    pub_cloud = nh.advertise<pcl::PCLPointCloud2>("cloud", 1);orsens.getRate()

    ros::Rate loop_rate(15);

    working = true;
    sensor_msgs::Image ros_left, ros_disp, ros_depth;

    while (nh.ok() && working)
    {
        ros::Time time = ros::Time::now();

        orsens.grabSensorData();
        Mat color = orsens.getLeft();
        Mat disp = orsens.getDisp();
        Mat depth = orsens.getDepth();

        if (publish_color && !color.empty())
        {
            fillImage(ros_left, "rgb8", color.rows, color.cols, 3 * color.cols, color.data);

            ros_left.header.stamp = time;
            ros_left.header.frame_id = "orsens_camera";

            pub_left.publish(ros_left);

             //l_info_msg.header.stamp = time;
            //  l_info_msg.header.frame_id = "stereo_camera";
            //  pub_info.publish(l_info_msg);
        }

        if (publish_disp && !disp.empty())
        {
            fillImage(ros_disp, "mono8", disp.rows, disp.cols, disp.cols, disp.data);

            ros_disp.header.stamp = time;

            pub_disp.publish(ros_disp);

        }

        if (publish_depth && !depth.empty())
        {
            fillImage(ros_depth, "mono16", depth.rows, depth.cols, 2*depth.cols, depth.data);

            ros_depth.header.stamp = time;

            pub_depth.publish(ros_depth);

        }

        ros::spinOnce();
        loop_rate.sleep();

    }

    sigint_handler(0);

}
