/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2compress_omx.cpp
** 
** Read YUV420 from a V4L2 capture -> compress in H264 using OMX -> write to a V4L2 output device
**
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <signal.h>

#include <fstream>

extern "C"
{
#include "bcm_host.h"
#include "ilclient.h"
}

#include "logger.h"

#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"
#include <sys/time.h>
#include "encode_omx.h"

using namespace std;

int stop=0;


/* ---------------------------------------------------------------------------
**  SIGINT handler
** -------------------------------------------------------------------------*/
void sighandler(int)
{ 
       printf("SIGINT\n");
       stop =1;
}

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) 
{	
	int verbose=2;
	const char *in_devname = "/dev/video0";	
	int width = 1920;
	int height = 1080;	
	int fps = 30;	
	int c = 0;
	V4l2Access::IoType ioTypeIn  = V4l2Access::IOTYPE_MMAP;

	ofstream output_file;

	while ((c = getopt (argc, argv, "hW:H:P:O:F:v::rw")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose = 1; if (optarg && *optarg=='v') verbose++;  break;
			case 'W':	width = atoi(optarg); break;
			case 'H':	height = atoi(optarg); break;
			case 'F':	fps = atoi(optarg); break;
			case 'r':	ioTypeIn  = V4l2Access::IOTYPE_READWRITE; break;			
			case 'O':	std::cout << "Saving to output file " << optarg << std::endl; output_file.open(optarg); break;
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] [-W width] [-H height] source_device dest_device" << std::endl;
				std::cout << "\t -v            : verbose " << std::endl;
				std::cout << "\t -vv           : very verbose " << std::endl;
				std::cout << "\t -W width      : V4L2 capture width (default "<< width << ")" << std::endl;
				std::cout << "\t -H height     : V4L2 capture height (default "<< height << ")" << std::endl;
				std::cout << "\t -F fps        : V4L2 capture framerate (default "<< fps << ")" << std::endl;
				std::cout << "\t -r            : V4L2 capture using read interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t -w            : V4L2 capture using write interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t source_device : V4L2 capture device (default "<< in_devname << ")" << std::endl;
				exit(0);
			}
		}
	}
	if (optind<argc)
	{
		in_devname = argv[optind];
		optind++;
	}	

	if (output_file.fail()) {
		std::cerr << "Can't open output file";
		exit(1);
	}	
		
	// initialize log4cpp
	initLogger(verbose);

	// init V4L2 capture interface
	V4L2DeviceParameters param(in_devname,V4L2_PIX_FMT_YUV420,width,height,fps,verbose);
	V4l2Capture* videoCapture = V4l2Capture::create(param, ioTypeIn);
	
	if (videoCapture == NULL)
	{	
		LOG(WARN) << "Cannot create V4L2 capture interface for device:" << in_devname; 
	}
	else
	{
			LOG(NOTICE) << "Start Capturing from " << in_devname; 
			bcm_host_init();
			
			OMX_BUFFERHEADERTYPE *      buf = NULL;
			OMX_BUFFERHEADERTYPE *      out = NULL;
			COMPONENT_T *               video_encode = NULL;

			ILCLIENT_T *client = encode_init(&video_encode);
			if (client)
			{
				encode_config_input(video_encode, videoCapture->getWidth(), videoCapture->getHeight(), 30, OMX_COLOR_FormatYUV420PackedPlanar);
				encode_config_output(video_encode, OMX_VIDEO_CodingAVC, 10000000);

				encode_config_activate(video_encode);		
				timeval tv;
				
				LOG(NOTICE) << "Start Compressing " << in_devname; 					
				signal(SIGINT,sighandler);
				while (!stop) 
				{
					tv.tv_sec=1;
					tv.tv_usec=0;
					int ret = videoCapture->isReadable(&tv);
					if (ret == 1)
					{		
						buf = ilclient_get_input_buffer(video_encode, 200, 0);
						if (buf != NULL)
						{
							/* fill it */
							int rsize = videoCapture->read((char*)buf->pBuffer, buf->nAllocLen);
							buf->nFilledLen = rsize;

							if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_encode), buf) != OMX_ErrorNone)
							{
								fprintf(stderr, "Error emptying buffer!\n");
							}
						}
							
						out = ilclient_get_output_buffer(video_encode, 201, 0);
						if (out != NULL)
						{ 
							OMX_ERRORTYPE r = OMX_FillThisBuffer(ILC_GET_HANDLE(video_encode), out);
							if (r != OMX_ErrorNone)
							{
								LOG(WARN) << "Error filling buffer:" << r; 
							}
							if (out->nFilledLen > 0)
							{
								output_file.write((char*)out->pBuffer, (int)out->nFilledLen); 
								LOG(DEBUG) << "Wrote " << out->nFilledLen << " bytes.";
								for(int i = 0; i < 5; i++) {
									//printf("%02x ", out->pBuffer[i]);	
								}
							} else {
								LOG(DEBUG) << "Encoded buffer 0";
							}
							
							out->nFilledLen = 0;
						}					
					}
					else if (ret == -1)
					{
						LOG(NOTICE) << "stop error:" << strerror(errno); 
						stop=1;
					}
				}
				LOG(DEBUG) << "Done.";
				encode_deactivate(video_encode);
				encode_deinit(video_encode, client);	
				output_file.close();		
		}
		
		delete videoCapture;
	}
	
	return 0;
}
