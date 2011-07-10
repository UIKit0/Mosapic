#include "HexaMosaic.hpp"
#include "Debugger.hpp"

#include <cmath>
#include <fstream>
#include <queue>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <limits>

HexaMosaic::HexaMosaic(
	rcString inSourceImage,
	rcString inDatabase,
	cInt inWidth,
	cInt inHeight,
	cInt inDimensions,
	cInt inMaxRadius
	):
	mSourceImage(inSourceImage),
	mDatabase(inDatabase),
	mWidth(inWidth),
	mHeight(inHeight),
	mDimensions(inDimensions),
	mMaxRadius(inMaxRadius)
{
}

void HexaMosaic::Create() {
	cv::Mat database;
	int num_files = 0, num_images = 0, rows = 0, tile_size = 0;

	// Load database
	cv::FileStorage fs(mDatabase + "/rawdata.yml", cv::FileStorage::READ);
	fs["num_files"] >> num_files;
	fs["num_images"] >> num_images;
	fs["tile_size"] >> tile_size;
	fs.release();

	cv::Mat tmp, roi;
	int current_row = 0;
	database.create(num_images, tile_size*tile_size*3, CV_8UC1);
	for (int i = 0; i < num_files; i++)
	{
		std::stringstream s;
		s << i;
		std::string entry = mDatabase + "rawdata_" + s.str() + ".yml";
		std::cout << "Loading `" << entry << "'..." << std::flush;
		fs.open(entry, cv::FileStorage::READ);
		rows = 0;
		fs["rows"] >> rows;
		for (int j = 0; j < rows; j++)
		{
			std::stringstream ss;
			ss << j;
			fs["img_" + ss.str()] >> tmp;
			roi = database.row(current_row);
			tmp.copyTo(roi);
			current_row++;
		}
		fs.release();
		std::cout << "[done]" << std::endl;
	}

	// Compute pca components from source image
	cv::Mat src_img = cv::imread(mSourceImage, 1);
	cv::Mat src_img_scaled, pca_data;
	cv::resize(src_img, src_img_scaled, cv::Size(mWidth*tile_size, mHeight*tile_size), 0.0, 0.0, CV_INTER_CUBIC);
	pca_data.create(mHeight*mWidth, tile_size*tile_size*3, CV_8UC1);
	cv::Rect roi_patch(0, 0, tile_size, tile_size);
	cv::Mat src_patch;
	std::vector<cv::Point2i> shuffled_coords;
	for (int i = 0; i < mHeight; i++)
	{
		roi_patch.y = i * tile_size;
		for (int j = 0; j < mWidth; j++)
		{
			roi_patch.x = j * tile_size;
			src_patch = src_img_scaled(roi_patch).clone();
			src_patch = src_patch.reshape(1, 1);
			cv::Mat entry = pca_data.row(i*mWidth+j);
			src_patch.copyTo(entry);
			shuffled_coords.push_back(cv::Point(j,i));
		}
	}

	std::cout << "Performing pca..." << std::flush;
	cv::PCA pca(pca_data, cv::Mat(), CV_PCA_DATA_AS_ROW, mDimensions);
	#ifdef DEBUG
	// Construct eigenvector images for debugging
	for (int i = 0; i < mDimensions; i++)
	{
		cv::Mat eigenvec;
		cv::normalize(pca.eigenvectors.row(i), eigenvec, 255, 0, cv::NORM_MINMAX);
		eigenvec = eigenvec.reshape(3, tile_size);
		std::stringstream s;
		s << i;
		std::string entry = "eigenvector-" + s.str() + ".jpg";
		cv::imwrite(entry, eigenvec);
	}
	#endif
	std::cout << "[done]" << std::endl;

	// Compress original image data
	std::cout << "Compress source image..." << std::flush;
	cv::Mat compressed_src_img;
	CompressData(pca, pca_data, compressed_src_img);
	std::cout << "[done]" << std::endl;

	// Compress database image data
	std::cout << "Compress database..." << std::flush;
	cv::Mat compressed_database;
	CompressData(pca, database, compressed_database);
	std::cout << "[done]" << std::endl;

	// Construct mosaic
	std::cout << "Construct mosaic..." << std::flush;
	random_shuffle(shuffled_coords.begin(), shuffled_coords.end());
	cv::Mat src_entry, tmp_entry, dst_patch;
	vInt ids;
	std::vector<cv::Point2i> locations;
	for (int i = 0; i < mHeight; i++)
	{
		for (int j = 0; j < mWidth; j++)
		{
			const cv::Point2i& loc = shuffled_coords[i*mWidth+j];
			roi_patch.x = loc.x * tile_size;
			roi_patch.y = loc.y * tile_size;
			src_entry = compressed_src_img.row(loc.y*mWidth+loc.x);
			// Match with database
			std::priority_queue<Match> KNN; // All nearest neighbours
			for (int k = 0; k < num_images; k++)
			{
				tmp_entry = compressed_database.row(k);
				float dist = GetDistance(src_entry, tmp_entry);
				KNN.push(Match(k, dist));
			}
			int best_id = KNN.top().id;
			while (!KNN.empty())
			{
				// Stop if the image is new
				if (find(ids.begin(), ids.end(), best_id) == ids.end())
					break;

				// Stop if the image has no duplicates in a certain radius
				cInt max_radius = 5;
				bool is_used = false;
				for (int k = 0, n = locations.size(); k < n; k++)
				{
					int x = locations[k].x-loc.x;
					int y = locations[k].y-loc.y;
					int radius = int(ceil(sqrt(x*x+y*y)));
					if (radius <= max_radius && ids[k] == best_id)
					{
						is_used = true;
						break;
					}
				}

				if (!is_used)
					break;
				KNN.pop();
				best_id = KNN.top().id;
			}
			ids.push_back(best_id);
			locations.push_back(loc);
			src_patch = src_img_scaled(roi_patch);
			dst_patch = database.row(best_id);
			dst_patch = dst_patch.reshape(3, tile_size);
			dst_patch.copyTo(src_patch);
		}
	}
	std::stringstream s;
	s << "mosaic-pca" << mDimensions
	  << "-tilesize"  << tile_size
	  << "-maxradius" << mMaxRadius
	  << ".jpg";

	cv::imwrite(s.str(), src_img_scaled);
	std::cout << "[done]" << std::endl;
	std::cout << "Resulting image: " << s.str() << std::endl;
}

void HexaMosaic::CompressData(const cv::PCA& inPca, const cv::Mat& inUnCompressed, cv::Mat& outCompressed) {
	outCompressed.create(inUnCompressed.rows, mDimensions, inPca.eigenvectors.type());
	cv::Mat entry, compressed_entry;
	for (int i = 0; i < inUnCompressed.rows; i++)
	{
		entry = inUnCompressed.row(i);
		compressed_entry = outCompressed.row(i);
		inPca.project(entry, compressed_entry);
	}
}

float HexaMosaic::GetDistance(const cv::Mat& inSrcRow, const cv::Mat& inDataRow) {
	cv::Mat diff, square;
	float sum, dist;
	cv::subtract(inSrcRow, inDataRow, diff);
	cv::pow(diff, 2.0, square);
	sum = cv::sum(square)[0];
	dist = sqrt(sum);
	return dist;
}
