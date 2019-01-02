/*
 * A simple 2D gridmap structure
 *
 * Copyright 2011 Armin Hornung, University of Freiburg
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of Freiburg nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "gridmap_2d/GridMap2D.h"
#include <ros/console.h>

namespace gridmap_2d{

GridMap2D::GridMap2D()
: m_frameId("/map")
{

}

GridMap2D::GridMap2D(const nav_msgs::OccupancyGridConstPtr& gridMap, bool unknown_as_obstacle) {

  setMap(gridMap, unknown_as_obstacle);

}

GridMap2D::GridMap2D(const GridMap2D& other)
 : m_binaryMap(other.m_binaryMap.clone()),
   m_distMap(other.m_distMap.clone()),
   m_mapInfo(other.m_mapInfo),
   m_frameId(other.m_frameId)
{

}

GridMap2D::~GridMap2D() {

}

void GridMap2D::updateDistanceMap(){
  cv::distanceTransform(m_binaryMap, m_distMap, CV_DIST_L2, CV_DIST_MASK_PRECISE);
  // distance map now contains distance in meters:
  m_distMap = m_distMap * m_mapInfo.resolution;
}

void GridMap2D::setMap(const nav_msgs::OccupancyGridConstPtr& grid_map, bool unknown_as_obstacle){
  m_mapInfo = grid_map->info;
  m_frameId = grid_map->header.frame_id;
  // allocate map structs so that x/y in the world correspond to x/y in the image
  // (=> cv::Mat is rotated by 90 deg, because it's row-major!)
  m_binaryMap = cv::Mat(m_mapInfo.width, m_mapInfo.height, CV_8UC1);
  m_distMap = cv::Mat(m_binaryMap.size(), CV_32FC1);

  std::vector<signed char>::const_iterator mapDataIter = grid_map->data.begin();

  //TODO check / param
  unsigned char map_occ_thres = 70;

  // iterate over map, store in image
  // (0,0) is lower left corner of OccupancyGrid
  for(unsigned int j = 0; j < m_mapInfo.height; ++j){
    for(unsigned int i = 0; i < m_mapInfo.width; ++i){
      if (*mapDataIter > map_occ_thres
          || (unknown_as_obstacle && *mapDataIter < 0))
      {
        m_binaryMap.at<uchar>(i,j) = OCCUPIED;
      } else{
        m_binaryMap.at<uchar>(i,j) = FREE;
      }
      ++mapDataIter;
    }
  }

  updateDistanceMap();

  ROS_INFO("GridMap2D created with %d x %d cells at %f resolution.", m_mapInfo.width, m_mapInfo.height, m_mapInfo.resolution);
}

nav_msgs::OccupancyGrid GridMap2D::toOccupancyGridMsg() const{
  nav_msgs::OccupancyGrid msg;
  msg.header.frame_id = m_frameId;
  msg.header.stamp = ros::Time::now();
  msg.info = m_mapInfo;
  msg.data.resize(msg.info.height*msg.info.width);

  // iterate over map, store in data
  std::vector<signed char>::iterator mapDataIter = msg.data.begin();
  // (0,0) is lower left corner of OccupancyGrid
  for(unsigned int j = 0; j < m_mapInfo.height; ++j){
    for(unsigned int i = 0; i < m_mapInfo.width; ++i){
      if (m_binaryMap.at<uchar>(i,j) == OCCUPIED)
        *mapDataIter = 100;
      else
        *mapDataIter = 0;

      ++mapDataIter;
    }
  }

  return msg;
}

void GridMap2D::setMap(const cv::Mat& binaryMap){
  m_binaryMap = binaryMap.clone();
  m_distMap = cv::Mat(m_binaryMap.size(), CV_32FC1);

  cv::distanceTransform(m_binaryMap, m_distMap, CV_DIST_L2, CV_DIST_MASK_PRECISE);
  // distance map now contains distance in meters:
  m_distMap = m_distMap * m_mapInfo.resolution;

  ROS_INFO("GridMap2D copied from existing cv::Mat with %d x %d cells at %f resolution.", m_mapInfo.width, m_mapInfo.height, m_mapInfo.resolution);

}

void GridMap2D::inflateMap(double inflationRadius){
  m_binaryMap = (m_distMap > inflationRadius );
  // recompute distance map with new binary map:
  cv::distanceTransform(m_binaryMap, m_distMap, CV_DIST_L2, CV_DIST_MASK_PRECISE);
  m_distMap = m_distMap * m_mapInfo.resolution;
}

// See costmap2D for mapToWorld / worldToMap implementations:

void GridMap2D::mapToWorld(unsigned int mx, unsigned int my, double& wx, double& wy) const {
  wx = m_mapInfo.origin.position.x + (mx+0.5) * m_mapInfo.resolution;
  wy = m_mapInfo.origin.position.y + (my+0.5) * m_mapInfo.resolution;
}



void GridMap2D::worldToMapNoBounds(double wx, double wy, unsigned int& mx, unsigned int& my) const {
  mx = (int) ((wx - m_mapInfo.origin.position.x) / m_mapInfo.resolution);
  my = (int) ((wy - m_mapInfo.origin.position.y) / m_mapInfo.resolution);
}

bool GridMap2D::worldToMap(double wx, double wy, unsigned int& mx, unsigned int& my) const {
  if(wx < m_mapInfo.origin.position.x || wy < m_mapInfo.origin.position.y)
    return false;

  mx = (int) ((wx - m_mapInfo.origin.position.x) / m_mapInfo.resolution);
  my = (int) ((wy - m_mapInfo.origin.position.y) / m_mapInfo.resolution);

  if(mx < m_mapInfo.width && my < m_mapInfo.height)
    return true;

  return false;
}

bool GridMap2D::inMapBounds(double wx, double wy) const{
  unsigned mx, my;
  return worldToMap(wx,wy,mx,my);
}

float GridMap2D::distanceMapAt(double wx, double wy) const{
  unsigned mx, my;

  if (worldToMap(wx, wy, mx, my))
    return bezier_distMap.at<float>(mx, my);
    //return m_distMap.at<float>(mx, my);
  else
    return -1.0f;
}
float GridMap2D::distanceMapAtStartGoal(double wx, double wy) const {
	unsigned mx, my;
	if (worldToMap(wx, wy, mx, my))
		//return m_distMap.at<float>(mx, my);
		return m_distMap.at<float>(mx, my);
	else
		return -1.0f;
}

uchar GridMap2D::binaryMapAt(double wx, double wy) const{
  unsigned mx, my;

  if (worldToMap(wx, wy, mx, my))
    return m_binaryMap.at<uchar>(mx, my);
  else
    return 0;
}

float GridMap2D::distanceMapAtCell(unsigned int mx, unsigned int my) const{
  return m_distMap.at<float>(mx, my);
}


uchar GridMap2D::binaryMapAtCell(unsigned int mx, unsigned int my) const{
  return m_binaryMap.at<uchar>(mx, my);
}

uchar& GridMap2D::binaryMapAtCell(unsigned int mx, unsigned int my){
  return m_binaryMap.at<uchar>(mx, my);
}


bool GridMap2D::isOccupiedAtCell(unsigned int mx, unsigned int my) const{
  //return (m_binaryMap.at<uchar>(mx, my) < 255);
  return (bezier_binaryMap.at<uchar>(mx, my) < 255);

}


bool GridMap2D::isOccupiedAt(double wx, double wy) const{
  unsigned mx, my;
  if (worldToMap(wx, wy, mx, my))
    return isOccupiedAtCell(mx, my);
  else
    return true;
}
bool GridMap2D::isOccupiedAtStartGoal(double wx, double wy) const {
	unsigned mx, my;
	if (worldToMap(wx, wy, mx, my))
		//return isOccupiedAtCell(mx, my);
		return (m_binaryMap.at<uchar>(mx, my) < 255);
	else
		return true;
}
float
	GridMap2D::Ni(int n, int i)
{
	float ni;
	ni = Factrl(n) / (Factrl(i)*Factrl(n - i));
	return ni;
}

// function to calculate the Bernstein basis
float
	GridMap2D::Basis(int n, int i, float t)
{
	float basis;
	float ti; /* this is t^i */
	float tni; /* this is (1 - t)^i */

	/* handle the special cases to avoid domain problem with pow */

	if (t == 0. && i == 0)
		ti = 1.0;
	else
		ti = pow(t, i);

	if (n == i && t == 1.)
		tni = 1.0;
	else
		tni = pow((1 - t), (n - i));
	basis = Ni(n, i)*ti*tni; /* calculate Bernstein basis function */
	return basis;
}

// Bezier curve subroutine
int
	GridMap2D::Bezier(Point2DTheta *sPoint, int inPointNum, Point2DTheta *sOutPoint, int outPointNum)
{

	float step;
	float t;

	/*    calculate the points on the Bezier curve */
	t = 0;
	step = 1.0f / ((float)(outPointNum - 1));

	for (int i1 = 0; i1 < outPointNum; i1++) /* main loop */
	{
		if ((1.0 - t) < 5e-6)
		{
			t = 1.0;
		}

		sOutPoint[i1].x = 0.0;
		sOutPoint[i1].y = 0.0;

		for (int posi = 0; posi < inPointNum; posi++) /* Do x,y,z components */
		{
			sOutPoint[i1].x = sOutPoint[i1].x + Basis(inPointNum - 1, posi, t)*sPoint[posi].x;
			sOutPoint[i1].y = sOutPoint[i1].y + Basis(inPointNum - 1, posi, t)*sPoint[posi].y;
		}
		t = t + step;
	}

	return 0;
}
int GridMap2D::updateBezierMap(std::pair<float, float> pointF0,std::pair<float, float> pointF1,std::pair<float, float> pointF2,std::pair<float, float> pointF3)
{
	getBezierF0(pointF0.first, pointF0.second);
	getBezierF1(pointF1.first, pointF1.second);
	getBezierF2(pointF2.first, pointF2.second);
	getBezierF3(pointF3.first, pointF3.second);
	//(Point2DTheta *sPoint, int inPointNum, Point2DTheta *sOutPoint, int outPointNum);
	ROS_INFO("Bezier F1 is (%f %f).\n", sPoint[0].x, sPoint[0].y);
	ROS_INFO("Bezier F2 is(%f %f).\n", sPoint[1].x, sPoint[1].y);
	ROS_INFO("Bezier F3 is (%f %f).\n", sPoint[2].x, sPoint[2].y);
	ROS_INFO("Bezier F4 is (%f %f).\n", sPoint[3].x, sPoint[3].y);

	int x_diff = abs(state_2_cell(sPoint[0].x, m_mapInfo.resolution) - state_2_cell(sPoint[3].x, m_mapInfo.resolution));
	int y_diff = abs(state_2_cell(sPoint[0].y, m_mapInfo.resolution) - state_2_cell(sPoint[3].y, m_mapInfo.resolution));
	// max = (a >= b && a >= c) ? a : (b >= a && b >= c) ? b : c;
	int outPointNum = x_diff > y_diff ? 2*x_diff : 2*y_diff;

	Point2DTheta sOutPoint[400]; //最大点数为图的weight height
	Bezier(sPoint, 4, sOutPoint, outPointNum);

	bezier_binaryMap = cv::Mat(m_mapInfo.width, m_mapInfo.height, CV_8UC1);
	bezier_distMap = cv::Mat(m_binaryMap.size(), CV_32FC1);
	for(unsigned int i=0;i<m_mapInfo.width;i++)
		for (unsigned int j = 0; j < m_mapInfo.height; j++)
		{
			bezier_binaryMap.at<uchar>(i, j) = FREE;
		}
	for (unsigned int j = 0; j < outPointNum; j++) {
		int x = state_2_cell(sOutPoint[j].x, m_mapInfo.resolution);
		int y = state_2_cell(sOutPoint[j].y,m_mapInfo.resolution);
		bezier_binaryMap.at<uchar>(x, y) = OCCUPIED;
	}
	cv::distanceTransform(bezier_binaryMap, bezier_distMap, CV_DIST_L2, CV_DIST_MASK_PRECISE);
	bezier_distMap = bezier_distMap * m_mapInfo.resolution;

	for (unsigned int i = 0; i < m_mapInfo.width; i++)
		for (unsigned int j = 0; j < m_mapInfo.height; j++)
		{
			if (bezier_distMap.at<float>(i, j) <= PASSWIDTH and m_binaryMap.at<uchar>(i,j)==FREE) //脚宽度
				bezier_binaryMap.at<uchar>(i, j) = FREE;
			else bezier_binaryMap.at<uchar>(i, j) = OCCUPIED;
		}
	cv::distanceTransform(bezier_binaryMap, bezier_distMap, CV_DIST_L2, CV_DIST_MASK_PRECISE);
	bezier_distMap = bezier_distMap * m_mapInfo.resolution;

	return 0;
}

}


