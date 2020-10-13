# python config
If wou want to use python 3
edit file nuitrack_ros/nuitrack_user_viewer/src/viewer_node.py.
On top of the file spacify python3 insted of python

# realsense parameters
copy nuitrack.config file to /usr/etc/nuitrack/data/ folder.
Before do that, please create buckup of original nuitrack.config

Edit file nuitrack_ros/launch/nuitrack.launch. Set the hight of realsense camera

<arg name="offset_z" default="0.87" />

# nuitrack_ros
ROS package for interfacing with nuitrack API


## Requirements

* Install openni2_launch (For using Asus Xtion, Kinect1, Astra Cameras, etc...)

        $ sudo apt install ros-kinetic-openni2-launch


* Install realsense packages (For using realsense)

        $ sudo apt-key adv --keyserver hkp://keys.gnupg.net:80 --recv-key C8B3A55A6F3EFCDE
        $ sudo add-apt-repository "deb http://realsense-hw-public.s3.amazonaws.com/Debian/apt-repo xenial main" -u
        $ sudo apt install librealsense2=2.11.1-0~realsense0.70
        $ sudo apt install librealsense2-dev=2.11.1-0~realsense0.70


* Install https://nuitrack.com and Download Nuitrack Pro for Linux
  visit http://download.3divi.com/Nuitrack/platforms,

        $ sudo dpkg -i nuitrack-ubuntu-amd64.deb


## Install

        $ git clone https://github.com/byeongkyu/nuitrack_ros.git
        $ catkin build


## Execute

* For openni2

        $ roslaunch nuitrack_ros bringup.launch


* For realsense

        $ roslaunch nuitrack_ros bringup.launch use_openni2:=false use_realsense:=true


## Pacakges

* [nuitrack_ros](./nuitrack_ros): meta package for nuitrack_ros
* [nuitrack_msgs](./nuitrack_msgs): define the messages for nuitrack_ros
* [nuitrack_core](./nuitrack_core): The core package for interfacing with nuitrack library
* [nuitrack_user_viewer](./nuitrack_user_viewer): The visualization package for nuitrack_ros
