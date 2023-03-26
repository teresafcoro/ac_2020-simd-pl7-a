/*
 * Main.cpp
 *
 * Created on: Fall 2019
 * 
 * Teamwork: 2020-simd-pl7-a
 */

#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <CImg.h>
#include <time.h>
#include <immintrin.h> // Required to use intrinsic functions

// Using the classes/functions of the cimg_library
using namespace cimg_library;

// Data type for image components
typedef float data_t;

// Pointers to the source image and the destination image
const char *SOURCE_IMG		= "/home/student/Pictures/normal/uniovi_2.bmp";
const char *DESTINATION_IMG = "/home/student/Pictures/uniovi_2_final.bmp";

// Number of items per packet
#define ITEMS_PER_PACKET (sizeof(__m128) / sizeof(data_t))

// Number os repetitions for the loop of the elapsed time
int nREPS = 30;

// Principal method of the class
int main() {

	// Try to load the source image
	// If it is not possible, catch the exception
	cimg::exception_mode(0);
	try {

		// Open file and object initialization
		CImg<data_t> srcImage(SOURCE_IMG);

		// Creating the variables
		data_t *pRsrc, *pGsrc, *pBsrc;	// Pointers to the R, G and B components
		data_t *pDstImage;	// Pointer to the new image pixels
		uint width, height;	// Width and height of the image
		uint nComp;	// Number of image components

		// Variables initialization
		srcImage.display();	// Displays the source image
		width = srcImage.width();	// Getting information from the source image
		height = srcImage.height();
		nComp = srcImage.spectrum();

		// Array size. Note: It is not a multiple of 8
		#define VECTOR_SIZE (width * height)

		// Allocate memory space for destination image components
		pDstImage = (data_t *)_mm_malloc(width * height * nComp * sizeof(data_t), sizeof(data_t));
		if (pDstImage == NULL) {
			perror("Allocating destination image");
			exit(-2);
		}

		// Pointers to the component arrays of the source image
		pRsrc = srcImage.data();		// pRcomp points to the R component array
		pGsrc = pRsrc + height * width;	// pGcomp points to the G component array
		pBsrc = pGsrc + height * width;	// pBcomp points to the B component array

		// Create the variables for the time measurement
		struct timespec tStart, tEnd;
		double dElapsedTimeS = 0;

		// Calculate the number of packets of the resulting array
		int nPackets = (VECTOR_SIZE * sizeof(data_t) / sizeof(__m128));

		// If it is not an exact number we need to add one more packet
		if (((VECTOR_SIZE * sizeof(data_t)) % sizeof(__m128)) != 0){
			nPackets++;
		}

		// 32 bytes (128 bits) packets. Used to stored aligned memory data
		__m128 vred, vgreen, vblue, vrg, vrgb;

		// Define the packets that multiply the components
		__m128 paquete03, paquete059, paquete011, paquete255;

		// Initialize those packets
		paquete03 = _mm_set1_ps(0.3);
		paquete059 = _mm_set1_ps(0.59);
		paquete011 = _mm_set1_ps(0.11);
		paquete255 = _mm_set1_ps(255);

		// Start time
		if (clock_gettime(CLOCK_REALTIME, &tStart) == -1) {
			printf("ERROR: clock_gettime: %d, \n", errno);
			exit(EXIT_FAILURE);
		}

		// Loop with the same number of repetitions as the single-thread version
		for (int i = 0; i < nREPS; i++) {
			// Data arrays vred, rgreen and vblue must not be memory aligned to __m128 data (32 bytes)
			// so we use intermediate variables to avoid execution errors
			// We make an unaligned load of vred, rgreen and vblue
			vred = _mm_loadu_ps(pRsrc);
			vgreen = _mm_loadu_ps(pGsrc);
			vblue = _mm_loadu_ps(pBsrc);

			// Begin the treatment of the first packet group
			// Multiply the components and the constants
			vred = _mm_mul_ps(vred, paquete03);
			vgreen = _mm_mul_ps(vgreen, paquete059);
			vblue = _mm_mul_ps(vblue, paquete011);

			// Performs the addition of two aligned vectors
			// each vector containing 128 bits / 32 bytes = 4 floats
			// Intermediate step to put together all the components
			vrg = _mm_add_ps(vred, vgreen);

			// Performs the addition of two aligned vectors
			// each vector containing 128 bits / 32 bytes = 4 floats
			// All the components are in dst img array with this operation
			*(__m128 *)pDstImage = _mm_add_ps(vrg, vblue);

			// Treatment of the rest of the packets
			for (int i = 0; i < nPackets; i++){
				// Load next packet
				vred = _mm_loadu_ps((pRsrc + ITEMS_PER_PACKET * i));
				vgreen = _mm_loadu_ps((pGsrc + ITEMS_PER_PACKET * i));
				vblue = _mm_loadu_ps((pBsrc + ITEMS_PER_PACKET * i));

				// Multiply the components and the constants
				vred = _mm_mul_ps(vred, paquete03);
				vgreen = _mm_mul_ps(vgreen, paquete059);
				vblue = _mm_mul_ps(vblue, paquete011);

				// Intermediate step to put all the components together
				vrg = _mm_add_ps(vred, vgreen);

				// Last step to put the components together
				vrgb = _mm_add_ps(vrg, vblue);

				// Save the info into the dst img
				*(__m128 *)(pDstImage + ITEMS_PER_PACKET * i) = _mm_sub_ps(paquete255, vrgb);
			}

			// If vectors have not a number of elements multiple of ITEMS_PER_PACKET
			// it is necessary to differentiate the last iteration

			// Calculate the elements in excess
			int dataInExcess = (VECTOR_SIZE) % (sizeof(__m128) / sizeof(data_t));

			// Surplus data can be processed sequentially
			for (int i = 0; i < dataInExcess; i++) {
				*(pDstImage + 2 * ITEMS_PER_PACKET + i) = *(pRsrc + 2 * ITEMS_PER_PACKET + i) +
														  *(pGsrc + 2 * ITEMS_PER_PACKET + i) +
														  *(pBsrc + 2 * ITEMS_PER_PACKET + i);
			}
		}

		// End time
		if (clock_gettime(CLOCK_REALTIME, &tEnd) == -1) {
			printf("ERROR: clock_gettime: %d, \n", errno);
			exit(EXIT_FAILURE);
		}

		// Calculate the elapsed time in the execution of the algorithm
		dElapsedTimeS = (tEnd.tv_sec - tStart.tv_sec);
		dElapsedTimeS += (tEnd.tv_nsec - tStart.tv_nsec) / 1e+9;

		// Using nComp = 1 for B/W images
		nComp = 1;

		// Create a new image object with the calculated pixels
		CImg<data_t> dstImage(pDstImage, width, height, 1, nComp);
		dstImage.save(DESTINATION_IMG);

		// Display destination image
		dstImage.display();

		// Showing the elapsed time
		printf("Elapsed time	: %f s.\n", dElapsedTimeS);

		// Free the memory allocated at the beginning
		_mm_free(pDstImage);

	} catch (CImgException &e) {
		std::fprintf(stderr, "Error while loading the source image: %s", e.what());
	}

	return 0;
}