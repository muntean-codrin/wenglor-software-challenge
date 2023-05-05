#include <iostream>
#include <opencv2/opencv.hpp>
#include <windows.h>
#include <vector>

#include "BS_thread_pool.hpp"


void processPart(cv::Mat& imagePart)
{
	cv::Mat img_absdiff;
	cv::Mat image_copy = imagePart.clone();
	cv::medianBlur(imagePart, imagePart, 3);

	// Compute the absolute difference between the image and its median blur
	cv::absdiff(image_copy, imagePart, img_absdiff);
	// Compute the mean absolute difference of the pixels
	// This is a measure of the amount of noise in the image part
	double mad = cv::mean(img_absdiff).val[0];
	// If the mean absolute difference is greater than a threshold it indicates gaussian noise
	if (mad > 33)
		cv::GaussianBlur(imagePart, imagePart, cv::Size(21, 21), 0);
}

void processImage(std::string filename)
{
	std::string input_name = "Input Files\\" + filename;
	std::string output_name = "Output Files\\output_" + filename.substr(6);;

	cv::Mat image = cv::imread(input_name, cv::IMREAD_GRAYSCALE);

	if (image.empty())
	{
		std::cout << "Could not read the image " << input_name << ". Skipping..." << std::endl;
		return;
	}

	int width = image.cols;
	int height = image.rows;

	int numPieces = 6; // represents both the number of rows and columns
	int totalPieces = numPieces * numPieces;

	int pieceWidth = width / numPieces;
	int pieceHeight = height / numPieces;

	std::vector<cv::Mat> imagePieces(totalPieces);

	// Cut the image into pieces that have the same aspect ratio as the original image.
	int pieceX, pieceY;
	for (int i = 0; i < numPieces - 1; i++)
	{
		pieceX = i * pieceWidth;
		for (int j = 0; j < numPieces - 1; j++)
		{
			pieceY = j * pieceHeight;
			cv::Rect pieceRect(pieceX, pieceY, pieceWidth, pieceHeight);
			imagePieces[i * numPieces + j] = image(pieceRect);
		}
		// Handle the edge cases separately to account for pieces that don't divide evenly into the image dimensions.
		// For the last row of pieces, create a piece that extends to the bottom edge of the image.
		pieceY = (numPieces - 1) * pieceHeight;
		cv::Rect pieceRect(pieceX, pieceY, pieceWidth, height - pieceY);
		imagePieces[i * numPieces + numPieces - 1] = image(pieceRect);
	}

	// For the last column of pieces, create a piece that extends to the right edge of the image.
	pieceX = (numPieces - 1) * pieceWidth;
	for (int j = 0; j < numPieces - 1; j++)
	{
		pieceY = j * pieceHeight;
		cv::Rect pieceRect(pieceX, pieceY, width - pieceX, pieceHeight);
		imagePieces[(numPieces - 1) * numPieces + j] = image(pieceRect);
	}

	// For the last square, create a piece that extends to the bottom and right edges of the image.
	pieceY = (numPieces - 1) * pieceHeight;
	cv::Rect pieceRect(pieceX, pieceY, width - pieceX, height - pieceY);
	imagePieces[numPieces * numPieces - 1] = image(pieceRect);


	// Thread pool for processing image parts in parallel
	BS::thread_pool pool;
	BS::multi_future<void> futures;
	for (int i = 0; i < totalPieces; ++i)
	{
		futures.push_back(pool.submit(processPart, std::ref(imagePieces[i])));
	}

	// Wait for the threadpool to execute all tasks
	futures.get();

	// Reconstruct the image by concatenating all the pieces together
	std::vector<cv::Mat> reconstructedColumns(numPieces);
	for (int i = 0; i < numPieces; i++) {
		std::vector<cv::Mat> imagesInSet(imagePieces.begin() + i * numPieces, imagePieces.begin() + (i + 1) * numPieces);
		cv::Mat concatenatedImages;
		cv::vconcat(imagesInSet, concatenatedImages);
		reconstructedColumns[i] = concatenatedImages;
	}

	cv::Mat reconstructedImage;
	cv::hconcat(reconstructedColumns, reconstructedImage);
	cv::imwrite(output_name, reconstructedImage);

	std::cout << "Image " << output_name << " saved" << std::endl;
}


int main(int argc, char* argv[])
{
	CreateDirectory(L"Output Files", NULL);
	WIN32_FIND_DATAA findData;
	HANDLE hFind;

	const char* filePattern = ".\\Input Files\\*.bmp";

	hFind = FindFirstFileA(filePattern, &findData);

	if (hFind == INVALID_HANDLE_VALUE) {
		std::cout << "Could not find any BMP files in the Input Files directory." << std::endl;
		return -1;
	}

	do {
		processImage(findData.cFileName);
	} while (FindNextFileA(hFind, &findData));

	FindClose(hFind);

	return 0;
}