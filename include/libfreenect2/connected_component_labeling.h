/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2014 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

#ifndef CONNECTED_COMPONENT_LABELING_H_
#define CONNECTED_COMPONENT_LABELING_H_

#include <string>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <libfreenect2/config.h>
#include <libfreenect2/frame_listener.hpp>

namespace libfreenect2
{

class LIBFREENECT2_API ConnectedComponentLabeling
{
public:
  ConnectedComponentLabeling();
  ~ConnectedComponentLabeling();

  /** Map color images onto depth images
   * @param depth Depth image (512x424 float)
   * @param[out] components Components for the depth image (512x424)
   */
  void apply(const Frame* depth, Frame* components, float threshold);
private:
  void setNeighbor(int *neighbors, int neighbor, int x, int y, int z, float threshold, const float *depth_data, unsigned int *components_data);
  int getMinLabel(int *neighbors, int *currentLabel);
  void saveEquivalences(int *neighbors, int minLabel, int *equivalences);
  struct Geometry {
    int pixels;
    long long sumX;
    long long sumY;
    int centroidX;
    int centroidY;
    long long sumDepth;
    int averageDepth;
    int guid;
  };
  Geometry *previous_components_geometry;
  int previous_max_label;
};

} /* namespace libfreenect2 */
#endif /* CONNECTED_COMPONENT_LABELING_H_ */
