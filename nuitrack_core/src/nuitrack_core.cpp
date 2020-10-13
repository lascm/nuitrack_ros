#include <ros/ros.h>
#include <nuitrack/Nuitrack.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/point_cloud2_iterator.h>
#include <sensor_msgs/PointCloud2.h>
#include <nuitrack_msgs/UserData.h>
#include <nuitrack_msgs/UserDataArray.h>
#include <nuitrack_msgs/EventUserUpdate.h>
#include <nuitrack_msgs/SkeletonData.h>
#include <nuitrack_msgs/SkeletonDataArray.h>
#include <sensor_msgs/fill_image.h>
#include <stdio.h>

using namespace tdv::nuitrack;
using namespace std;

std::map<int, std::string> JOINT_NAMES = {{JOINT_HEAD, "joint_head"}, {JOINT_NECK, "joint_neck"}, {JOINT_TORSO, "joint_torso"}, {JOINT_WAIST, "joint_waist"},
                                          {JOINT_LEFT_COLLAR, "joint_left_collar"}, {JOINT_LEFT_SHOULDER, "joint_left_shoulder"}, {JOINT_LEFT_ELBOW, "joint_left_elbow"}, {JOINT_LEFT_WRIST, "joint_left_wrist"}, {JOINT_LEFT_HAND, "joint_left_hand"},
                                          {JOINT_RIGHT_COLLAR, "joint_right_collar"}, {JOINT_RIGHT_SHOULDER, "joint_right_shoulder"}, {JOINT_RIGHT_ELBOW, "joint_right_elbow"}, {JOINT_RIGHT_WRIST, "joint_right_wrist"}, {JOINT_RIGHT_HAND, "joint_right_hand"},
                                          {JOINT_LEFT_HIP, "joint_left_hip"}, {JOINT_LEFT_KNEE, "joint_left_knee"}, {JOINT_LEFT_ANKLE, "joint_left_ankle"},
                                          {JOINT_RIGHT_HIP, "joint_right_hip"}, {JOINT_RIGHT_KNEE, "joint_right_knee"}, {JOINT_RIGHT_ANKLE, "joint_right_ankle"}};

class NuitrackCore
{
ros::Time stampG;

public:
    NuitrackCore(ros::NodeHandle nh)
    {
        pub_depth_data_ = nh.advertise<sensor_msgs::Image>("/nuitrack/depth/depth", 1); 
        pub_rgb_data_ = nh.advertise<sensor_msgs::Image>("/nuitrack/rgb/image_raw", 1);
        pub_pcl_data_ = nh.advertise<sensor_msgs::PointCloud2>("/nuitrack/depth/points", 1);
        pub_skeleton_data_ = nh.advertise<nuitrack_msgs::SkeletonDataArray>("/nuitrack/skeletons", 1);
        pub_user_data_ = nh.advertise<nuitrack_msgs::UserDataArray>("/nuitrack/detected_users", 5);
        pub_event_person_appeared_ = nh.advertise<nuitrack_msgs::EventUserUpdate>("/nuitrack/event/person_appeared", 5);
        pub_event_person_disappeared_ = nh.advertise<nuitrack_msgs::EventUserUpdate>("/nuitrack/event/person_disappeared", 5);

        try
        {
            Nuitrack::init();
        }
        catch (const Exception& e) {} // Do nothing

        colorSensor_ = ColorSensor::create();
        colorSensor_->connectOnNewFrame(std::bind(&NuitrackCore::onNewRGBFrame, this, std::placeholders::_1));

        depthSensor_ = DepthSensor::create();
        depthSensor_->connectOnNewFrame(std::bind(&NuitrackCore::onNewDepthFrame, this, std::placeholders::_1));

        OutputMode colorOutputMode = colorSensor_->getOutputMode();
        
        width_ = colorOutputMode.xres;
        height_ = colorOutputMode.yres;

        userTracker_ = UserTracker::create();
        userTracker_->connectOnNewUser(std::bind(&NuitrackCore::onNewUser, this, std::placeholders::_1));
        userTracker_->connectOnLostUser(std::bind(&NuitrackCore::onLostUser, this, std::placeholders::_1));
        userTracker_->connectOnUpdate(std::bind(&NuitrackCore::onUserUpdate, this, std::placeholders::_1));

        skeletonTracker_ = SkeletonTracker::create();
        skeletonTracker_->connectOnUpdate(std::bind(&NuitrackCore::onSkeletonUpdate, this, std::placeholders::_1));

        try
        {
            Nuitrack::run();
        }
        catch (const Exception& e)
        {
            std::cerr << "Can not start Nuitrack (ExceptionType: " << e.type() << ")" << std::endl;
            assert(false);
        }

        timer_ = nh.createTimer(ros::Duration(1/30), &NuitrackCore::timerCallback, this);
        ROS_INFO("Initialized nuitrack_core...");
    }

    ~NuitrackCore()
    {
        try
        {
            timer_.stop();
            Nuitrack::release();
        }
        catch (const Exception& e) {} // Do nothing
    }

private:
    void timerCallback(const ros::TimerEvent& event)
    {
        try
        {
            stampG=ros::Time::now();
            Nuitrack::update(colorSensor_);
            Nuitrack::update(depthSensor_);
            Nuitrack::update(userTracker_);
            Nuitrack::update(skeletonTracker_);
        }
        catch (LicenseNotAcquiredException& e)
        {
            std::cerr << "LicenseNotAcquired exception (ExceptionType: " << e.type() << ")" << std::endl;
            assert(false);
        }
        catch (const Exception& e)
        {
            std::cerr << "Nuitrack update failed (ExceptionType: " << e.type() << ")" << std::endl;
            assert(false);
        }
    }

    void onNewUser(int id)
    {
        current_user_list_.push_back(id);

        nuitrack_msgs::EventUserUpdate msg;
        msg.key_id = id;
        msg.user_ids = current_user_list_;

        pub_event_person_appeared_.publish(msg);
    }

    void onLostUser(int id)
    {
        bool found_exist = false;
        for(size_t i = 0; i < current_user_list_.size(); i++)
        {
            if(current_user_list_[i] == id)
            {
                current_user_list_.erase(current_user_list_.begin() + i);
                found_exist = true;
                break;
            }
        }
        if(!found_exist)
            return;

        nuitrack_msgs::EventUserUpdate msg;
        msg.key_id = id;
        msg.user_ids = current_user_list_;

        pub_event_person_disappeared_.publish(msg);
    }

    void onUserUpdate(UserFrame::Ptr frame)
    {
        nuitrack_msgs::UserDataArray msg;
        std::vector<User> users = frame->getUsers();

        int width = frame->getCols();
        int height = frame->getRows();

        for(size_t i = 0; i < users.size(); i++)
        {
            nuitrack_msgs::UserData user;

            user.id = users[i].id;
            user.real.x = users[i].real.x;  user.real.y = users[i].real.y; user.real.z = users[i].real.z;
            user.proj.x = users[i].proj.x;  user.proj.y = users[i].proj.y; user.proj.z = users[i].proj.z;
            user.box.x_offset = users[i].box.left * width;
            user.box.y_offset = users[i].box.top * height;
            user.box.height = (users[i].box.bottom - users[i].box.top) * height;
            user.box.width = (users[i].box.right - users[i].box.left) * width;
            user.occlusion = users[i].occlusion;

            msg.users.push_back(user);
        }

        pub_user_data_.publish(msg);
    }

    void onNewRGBFrame(RGBFrame::Ptr frame)
    {
        const Color3* colorPtr = frame->getData();

        sensor_msgs::Image img;
        //img.header.stamp = ros::Time::now();
        //ros::Time stampN;
        //stampN.fromNSec(frame->getTimestamp());
        //img.header.stamp = stampN;
        img.header.stamp = stampG;

        img.width = frame->getCols();
        img.height = frame->getRows();
        img.encoding = "rgb8";
        img.is_bigendian = 0;
        img.step = 3 * frame->getCols();

        img.data.resize(img.width * img.height * 3);

        for(size_t i = 0; i < img.width * img.height; i++)
        {
            img.data[i * 3 + 0] = colorPtr[i].blue;
            img.data[i * 3 + 1] = colorPtr[i].green;
            img.data[i * 3 + 2] = colorPtr[i].red;
        }

        pub_rgb_data_.publish(img);
    }

    sensor_msgs::PointCloud2 depthToCloud(int width, int height, const uint16_t* depth)
    {
        sensor_msgs::PointCloud2 cloud;
        cloud.header.frame_id = "nuitrack_link";
        //cloud.header.stamp = ros::Time::now();
        cloud.header.stamp = stampG;

        cloud.is_bigendian = false;
        cloud.is_dense = false;

        sensor_msgs::PointCloud2Modifier modifier(cloud);
        modifier.setPointCloud2FieldsByString(1, "xyz");
        modifier.resize(width * height);

        cloud.width = width;
        cloud.height = height;
        cloud.row_step = 16 * width;

        sensor_msgs::PointCloud2Iterator<float> out_x(cloud, "x");
        sensor_msgs::PointCloud2Iterator<float> out_y(cloud, "y");
        sensor_msgs::PointCloud2Iterator<float> out_z(cloud, "z");

        for(int h = 0; h < height; h++)
        {
            for(int w = 0; w < width; w++)
            {
                Vector3 data = depthSensor_->convertProjToRealCoords(w, h, depth[h * width + w]);

                *out_x = data.z / 1000.0;
                *out_y = -data.x / 1000.0;
                *out_z = data.y / 1000.0;

                ++out_x;
                ++out_y;
                ++out_z;
            }
        }

        return cloud;
    }

    void onNewDepthFrame(DepthFrame::Ptr frame)
    {
        const uint16_t* depthPtr = frame->getData();
        
        
        sensor_msgs::Image img;
        //img.header.stamp = ros::Time::now();
        //ros::Time stampN;
        //stampN.fromNSec(frame->getTimestamp());
        //img.header.stamp = stampN;
        img.header.stamp = stampG;

        img.width = frame->getCols();
        img.height = frame->getRows();
        //img.encoding = "16UC1";
        img.encoding= "8UC1";
        int maxDepth=5000;
        img.is_bigendian = 0;
        //img.step = 2 * img.width;
        img.step = img.width;
        //img.data.resize(img.width*img.height*2);
        img.data.resize(img.width*img.height);

        for(size_t i = 0; i < img.width * img.height; i++){
            //img.data[i * 2 + 0] = depthPtr[i] ;
            
            if (depthPtr[i]>maxDepth)
                img.data[i]=255;
            else
                img.data[i] = int(round(255*depthPtr[i]/maxDepth)) ;
            //cout<<"depth "<<img.data[i]<<endl;
            //img.data[i * 2 + 1] = depthPtr[i] >> 8;
        }

        pub_depth_data_.publish(img);

        sensor_msgs::PointCloud2 points = depthToCloud(frame->getCols(), frame->getRows(), depthPtr);
        pub_pcl_data_.publish(points);
    }

    void onSkeletonUpdate(SkeletonData::Ptr skeletonData)
    {
        nuitrack_msgs::SkeletonDataArray msg;
        //msg.header.stamp = ros::Time::now();
        //ros::Time stampN;
        //stampN.fromNSec(skeletonData->getTimestamp());
        //msg.header.stamp = stampN;
        msg.header.stamp = stampG;
        msg.header.frame_id = 'nuitrack_link';

        auto skeletons = skeletonData->getSkeletons();
        for(size_t i = 0; i < skeletonData->getNumSkeletons(); i++)
        {
            nuitrack_msgs::SkeletonData data;

            data.id = skeletons[i].id;

            for(auto const& j : JOINT_NAMES)
            {
                data.joints.push_back(j.second);

                geometry_msgs::Point p;
                p.x = skeletons[i].joints[j.first].real.x;
                p.y = skeletons[i].joints[j.first].real.y;
                p.z = skeletons[i].joints[j.first].real.z;

                data.joint_pos.push_back(p);
            }

            msg.skeletons.push_back(data);
        }

        pub_skeleton_data_.publish(msg);
    }

private:
    ros::Timer timer_;
    ros::Publisher pub_depth_data_;
    ros::Publisher pub_rgb_data_;
    ros::Publisher pub_pcl_data_;
    ros::Publisher pub_user_data_;
    ros::Publisher pub_skeleton_data_;
    ros::Publisher pub_event_person_appeared_;
    ros::Publisher pub_event_person_disappeared_;

    int width_;
    int height_;
    std::vector<int> current_user_list_;

    ColorSensor::Ptr colorSensor_;
    DepthSensor::Ptr depthSensor_;
    UserTracker::Ptr userTracker_;
    SkeletonTracker::Ptr skeletonTracker_;
    // HandTracker::Ptr handTracker_;
    // GestureRecognizer::Ptr gestureRecognizer_;
};

int main(int argc, char **argv)
{
    ros::init(argc, argv, "nuitrack_core");
    ros::NodeHandle nh;
    NuitrackCore m(nh);
    ros::spin();

    return 0;
}
