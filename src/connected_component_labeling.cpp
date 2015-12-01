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

#include <libfreenect2/connected_component_labeling.h>
#include <libfreenect2/logging.h>

namespace libfreenect2
{

void ConnectedComponentLabeling::apply(const Frame *depth, Frame *components, float threshold)
{
  // Check if all frames are valid and have the correct size
  if (!depth || !components ||
      depth->width != 512 || depth->height != 424 || depth->bytes_per_pixel != 4 ||
      components->width != 512 || components->height != 424 || components->bytes_per_pixel != 4)
    return;
  int neighbors[4];
  int currentLabel = 1;
  int equivalences[424 * 512 + 1] = {0};
  
  const float *depth_data = (float*)depth->data;
  unsigned int *components_data = (unsigned int*)components->data;

  // Pass 1: Label-like neighbors
  for (int y = 0; y < 424; y++) {
    for (int x = 0; x < 512; x++) {
      int index = y * 512 + x;
      float z = depth_data[index];
      if (z == 0) {
        components_data[index] = 0;
        continue;
      }

      setNeighbor(neighbors, 0, x-1, y, z, threshold, depth_data, components_data);
      setNeighbor(neighbors, 1, x, y-1, z, threshold, depth_data, components_data);
      setNeighbor(neighbors, 2, x-1, y-1, z, threshold, depth_data, components_data);
      setNeighbor(neighbors, 3, x+1, y-1, z, threshold, depth_data, components_data);

      int minLabel = getMinLabel(neighbors, &currentLabel);
      saveEquivalences(neighbors, minLabel, equivalences);
      components_data[index] = (unsigned int)minLabel;
    }
  }

  // Pass 2: Recursively unwind equivalences
  for (int i = 0; i < 424 * 512 + 1; i++) {
    int label = equivalences[i];
    if (equivalences[label] == 0) continue;
    do {
      label = equivalences[label];
    } while (equivalences[label] != 0);
    equivalences[i] = label;
  }

  // Pass 3: Update any components to their normalized equivalent
  //     & reorder labels to be in contiguous order
  int contiguous_equivalences[424 * 512 + 1] = {0};
  int max_label = 0;
  for (int y = 0; y < 424; y++) {
    for (int x = 0; x < 512; x++) {
      int index = y * 512 + x;
      int label = components_data[index];
      if (label == 0) continue;
      if (equivalences[label] != 0) {
        label = equivalences[label];
      }
      if (contiguous_equivalences[label] == 0) {
        contiguous_equivalences[label] = ++max_label;
      }
      components_data[index] = contiguous_equivalences[label];
    }
  }

  /* PAST THIS IS NOT CCL SPECIFIC */

  // Pass 4: Gather geometry for each component

  Geometry *components_geometry = new Geometry[max_label+1];
  memset(components_geometry, 0, sizeof(Geometry) * (max_label+1));
  for (int y = 0; y < 424; y++) {
    for (int x = 0; x < 512; x++) {
      int index = y * 512 + x;
      int label = components_data[index];
      components_geometry[label].pixels++;
      components_geometry[label].sumX += x;
      components_geometry[label].sumY += y;
      components_geometry[label].centroidX = components_geometry[label].sumX / components_geometry[label].pixels;
      components_geometry[label].centroidY = components_geometry[label].sumY / components_geometry[label].pixels;
      components_geometry[label].sumDepth += depth_data[index];
      components_geometry[label].averageDepth = components_geometry[label].sumDepth / components_geometry[label].pixels;
      if (components_geometry[label].guid == 0) {
        components_geometry[label].guid = rand() + 1;
      }
    }
  }

  // Pass 5: Check to see if the geometry fingerprint of each label is close to the same place as the previous one
  if (previous_components_geometry != NULL) {
  for (int i = 1; i <= max_label; i++) {
    Geometry *geometry = &components_geometry[i];
    for (int j = 1; j <= previous_max_label; j++) {
      Geometry *prev_geometry = &previous_components_geometry[j];
      if (abs(prev_geometry->centroidX-geometry->centroidX) < 25 && abs(prev_geometry->centroidY-geometry->centroidY) < 25
        && ((float)std::min(geometry->averageDepth, prev_geometry->averageDepth)/std::max(geometry->averageDepth, prev_geometry->averageDepth)) > 0.95
      /*&& ((float)std::min(geometry->pixels, prev_geometry->pixels)/std::max(geometry->pixels, prev_geometry->pixels)) > 0.85*/) {
        geometry->guid = prev_geometry->guid;
      }
    }
  }
  }

  delete[] previous_components_geometry;
  previous_components_geometry = components_geometry;
  previous_max_label = max_label;

  // Gather average depth and average pixels of all objects
  int sum_depth_overall = 0;
  int sum_pixels_overall = 0;
  for (int i = 1; i <= max_label; i++) {
    sum_depth_overall += components_geometry[i].averageDepth;
    sum_pixels_overall += components_geometry[i].pixels;
  }
  int average_depth_overall = sum_depth_overall / max_label;
  int average_pixels_overall = sum_pixels_overall / max_label;

  // Pass 6: Reject labels with num pixels < N
  for (int y = 0; y < 424; y++) {
    for (int x = 0; x < 512; x++) {
      int index = y * 512 + x;
      int label = components_data[index];
      if (label == 0) continue;
      if (components_geometry[label].pixels <= 100) {
        components_data[index] = 0;
        continue;
      }
    }
  }

  // Pass 7: Scale the label across all 4 bytes for graphics, remove background, and mark the centroid of each object
  for (int y = 0; y < 424; y++) {
    for (int x = 0; x < 512; x++) {
      int index = y * 512 + x;
      int label = components_data[index];
      if (label == 0) continue;
      Geometry *geometry = &components_geometry[label];

      // Eliminate background
      if (geometry->averageDepth > (0.85*average_depth_overall) && geometry->pixels > (0.85*average_pixels_overall)) {
        continue;
      }

      // Mark the centroid
      int centroidIndex = (geometry->centroidY * 512) + geometry->centroidX;
      if (centroidIndex == index) {
        components_data[index] = 0xFF0000;
        continue;
      }

      //int scaledLabel = ((float)label/max_label)*0xFFFFFF;
      //int scaledLabel = ((float)centroidIndex/(424 * 512))*0xFFFFFF;
      int scaledLabel = ((float)components_geometry[label].guid/RAND_MAX)*0xFFFFFF;
      components_data[index] = scaledLabel;
    }
  }
}

void ConnectedComponentLabeling::setNeighbor(int *neighbors, int neighbor, int x, int y, int z, float threshold, const float *depth_data, unsigned int *components_data) {
  int index = y * 512 + x;
  if (x < 0 || y < 0 || x >= 512 || y >= 424 || depth_data[index] == 0 || std::abs(depth_data[index]-z) >= threshold) {
    neighbors[neighbor] = -1;
  } else {
    neighbors[neighbor] = components_data[index];
  }
}

int ConnectedComponentLabeling::getMinLabel(int *neighbors, int *currentLabel) {
  int minLabel = neighbors[0];
  for (int i = 1; i < 4; i++) {
    if (neighbors[i] != -1 && neighbors[i] < minLabel) {
      minLabel = neighbors[i];
    }
  }
  if (minLabel == -1) {
    minLabel = ++(*currentLabel);
  }
  return minLabel;
}

void ConnectedComponentLabeling::saveEquivalences(int *neighbors, int minLabel, int *equivalences) {
  for (int i = 1; i < 4; i++) {
    if (neighbors[i] != -1 && neighbors[i] > minLabel) {
      if (equivalences[neighbors[i]] == 0 || minLabel < equivalences[neighbors[i]]) {
        equivalences[neighbors[i]] = minLabel;
      }
    }
  }
}

ConnectedComponentLabeling::ConnectedComponentLabeling()
{
  previous_components_geometry = NULL;
  previous_max_label = 0;
}

ConnectedComponentLabeling::~ConnectedComponentLabeling()
{
  if (previous_components_geometry != 0) {
    delete[] previous_components_geometry;
  }
}

} /* namespace libfreenect2 */
