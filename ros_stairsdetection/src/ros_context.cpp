#include "ros_context.hpp"

#include <geometry_msgs/PolygonStamped.h>
#include <geometry_msgs/Point32.h>
#include <geometry_msgs/TransformStamped.h>

void ROSContext::init(int argc, char **argv, void (*callback)(const sensor_msgs::PointCloud2ConstPtr&),
		bool (*exportStairs)(ros_stairsdetection::ExportStairs::Request&,
			ros_stairsdetection::ExportStairs::Response&),
		bool (*importStairs)(ros_stairsdetection::ImportStairs::Request&,
			ros_stairsdetection::ImportStairs::Response&),
		bool (*clearStairs)(ros_stairsdetection::ClearStairs::Request&,
			ros_stairsdetection::ClearStairs::Response&),
		std::vector<Stairway> *global_stairs) {

	/*
	 * load parameters from launch file
	 */
	std::string inputSetting;
	std::string stepsSetting;
	std::string stairsSetting;
	bool useSampleDataSetting;

	ros::init(argc, argv, "stairsdetection");
	ros::NodeHandle nh;

	ros::param::get("~input",  inputSetting);
	ros::param::get("~steps", stepsSetting);
	ros::param::get("~stairs", stairsSetting);

	ros::param::get("~publish_steps", m_publishStepsSetting);
	ros::param::get("~publish_stairs", m_publishStairsSetting);

	ros::param::get("~segmentation_iterations", m_segmentationIterationSetting);
	ros::param::get("~segmentation_threshold", m_segmentationThresholdSetting);

	ros::param::get("~max_step_width", m_maxStepWidthSetting);
	ros::param::get("~min_step_height", m_minStepHeightSetting);
	ros::param::get("~max_step_height", m_maxStepHeightSetting);

	ros::param::get("~camera_frame", m_cameraFrameSetting);
	ros::param::get("~robot_frame", m_robotFrameSetting);
	ros::param::get("~world_frame", m_worldFrameSetting);
	ros::param::get("~namespace", m_namespaceSetting);

	ros::param::get("~use_sample_data", useSampleDataSetting);

	tf2_ros::Buffer tfBuffer;
	tf2_ros::TransformListener tfListener(tfBuffer);
	m_th = TransformHelper(m_cameraFrameSetting, m_robotFrameSetting, m_worldFrameSetting, &tfBuffer);

	/*
	 * Init subscriber and listener
	 */
	ros::Subscriber sub = nh.subscribe<sensor_msgs::PointCloud2>(inputSetting.c_str(), 1, callback);
	m_pubSteps  = nh.advertise<visualization_msgs::MarkerArray>(stepsSetting.c_str(), 0);
	m_pubStairs = nh.advertise<visualization_msgs::MarkerArray>(stairsSetting.c_str(), 0);

	/*
	 * Init service get receive located stairs
	 */
	m_exportService = nh.advertiseService("export_stairs", exportStairs);
	m_importService = nh.advertiseService("import_stairs", importStairs);
	m_clearService  = nh.advertiseService("clear_stairs", clearStairs);

	// add test data
	if (useSampleDataSetting) {
		for (unsigned int i = 0; i < 3; i++) {
			Stairway s;
			Plane p1(pcl::PointXYZ(1*i, 2*i, 3*i),       pcl::PointXYZ(1.5*i, 2.5*i, 3.5*i));
			Plane p2(pcl::PointXYZ(1.1*i, 2.1*i, 3.1*i), pcl::PointXYZ(1.4*i, 2.4*i, 3.4*i));
			Plane p3(pcl::PointXYZ(1.2*i, 2.2*i, 3.2*i), pcl::PointXYZ(1.3*i, 2.3*i, 3.3*i));

			s.getSteps().push_back(p1);
			s.getSteps().push_back(p2);
			s.getSteps().push_back(p3);

			global_stairs->push_back(s);
		}
	}

	ros::spin();
}

void ROSContext::buildRosMarkerSteps(visualization_msgs::Marker &marker, std::vector<Plane> &planes,
		float (&color)[3]) {

	marker.header.frame_id = m_cameraFrameSetting.c_str();
	marker.header.stamp = ros::Time::now();
	marker.ns = m_namespaceSetting.c_str();
	marker.id = 0;
	marker.lifetime = ros::Duration();

	marker.type = visualization_msgs::Marker::LINE_LIST;
	marker.action = visualization_msgs::Marker::ADD;

	marker.scale.x = 0.05f;
	marker.color.r = color[0];
	marker.color.g = color[1];
	marker.color.b = color[2];
	marker.color.a = 1.0;

	for (std::vector<Plane>::iterator it = planes.begin(); it != planes.end(); it++) {

		std::vector<pcl::PointXYZ> points;
		m_th.buildStepFromAABB(*it, points);

		geometry_msgs::Point p1;
		m_th.transformPCLPointToROSPoint(points.at(0), p1);

		geometry_msgs::Point p2;
		m_th.transformPCLPointToROSPoint(points.at(1), p2);

		geometry_msgs::Point p3;
		m_th.transformPCLPointToROSPoint(points.at(2), p3);

		geometry_msgs::Point p4;
		m_th.transformPCLPointToROSPoint(points.at(3), p4);

		marker.points.push_back(p1);
		marker.points.push_back(p2);
		marker.points.push_back(p2);
		marker.points.push_back(p3);
		marker.points.push_back(p3);
		marker.points.push_back(p4);
		marker.points.push_back(p4);
		marker.points.push_back(p1);

		ROS_INFO("%s", it->toString().c_str());
	}
}

void ROSContext::buildROSMarkerStairs(visualization_msgs::Marker &marker, Stairway &stairway, float (&color)[3]) {
	
	// draw front of the steps
	buildRosMarkerSteps(marker, stairway.getSteps(), color);

	// draw surface of the steps
	if (stairway.getSteps().size() > 0) {
		for (unsigned int i = 1; i < stairway.getSteps().size(); i++) {
			std::vector<pcl::PointXYZ> pointsCur;
			m_th.buildStepFromAABB(stairway.getSteps().at(i), pointsCur);
			geometry_msgs::Point pc1;
			m_th.transformPCLPointToROSPoint(pointsCur.at(0), pc1);
			geometry_msgs::Point pc2;
			m_th.transformPCLPointToROSPoint(pointsCur.at(1), pc2);
			geometry_msgs::Point pc3;
			m_th.transformPCLPointToROSPoint(pointsCur.at(2), pc3);
			geometry_msgs::Point pc4;
			m_th.transformPCLPointToROSPoint(pointsCur.at(3), pc4);

			std::vector<pcl::PointXYZ> pointsBefore;
			m_th.buildStepFromAABB(stairway.getSteps().at(i - 1), pointsBefore);
			geometry_msgs::Point pb1;
			m_th.transformPCLPointToROSPoint(pointsBefore.at(0), pb1);
			geometry_msgs::Point pb2;
			m_th.transformPCLPointToROSPoint(pointsBefore.at(1), pb2);
			geometry_msgs::Point pb3;
			m_th.transformPCLPointToROSPoint(pointsBefore.at(2), pb3);
			geometry_msgs::Point pb4;
			m_th.transformPCLPointToROSPoint(pointsBefore.at(3), pb4);

			/*
			 * Get vertices of the rectangle
			 *
			 *  p2-----------------p3
			 *  |                   |
			 *  |                   |
			 *  p1-----------------p4
			 *
			 */

			marker.points.push_back(pc1);
			marker.points.push_back(pb2);
			marker.points.push_back(pc4);
			marker.points.push_back(pb3);
		}
	}
}

/**
 * Shows stairs in RVIZ
 */
void ROSContext::publishStairs(std::vector<Stairway> &stairway) {

	if (m_publishStairsSetting) {
		visualization_msgs::MarkerArray markerArray;

		for (std::vector<Stairway>::iterator it = stairway.begin(); it != stairway.end(); it++) {
			visualization_msgs::Marker marker;
			float color[3];
			color[0] = color[2] = 0.f;
			color[1] = 1.f;
			buildROSMarkerStairs(marker, *it, color);
			markerArray.markers.push_back(marker);
		}

		m_pubStairs.publish(markerArray);
	}
}

void ROSContext::publishSteps(std::vector<Plane> &planes) {
	visualization_msgs::MarkerArray markerArray;
	visualization_msgs::Marker marker;
	float color[3];
	color[0] = color[1] = 0.f;
	color[2] = 1.f;

	buildRosMarkerSteps(marker, planes, color);
	markerArray.markers.push_back(marker);
	m_pubSteps.publish(markerArray);
}
