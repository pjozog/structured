//
// stereo_feature_finder_test.cpp
//
// A program to test the feature finding classes by running through a set of
// images loaded from a contents file.
//
// Each line of the contents file should have the following format:
//    <timestamp> <left_image_name> <right_image_name>
//
// Ian Mahon
// 06-03-05
//

#include <cmath>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/time.h>
#include <time.h>

#include "cv.h"
#include "highgui.h"
#include "ulapack/eig.hpp"
#include "auv_image_distortion.hpp"
#include "auv_stereo_geometry.hpp"
#include "adt_raw_file.hpp"
#include "auv_stereo_corner_finder.hpp"
#include "auv_stereo_ncc_corner_finder.hpp"
#include "auv_stereo_keypoint_finder.hpp"
#include "auv_mesh.hpp"
#include "auv_mesh_io.hpp"

using namespace std;
using namespace libplankton;
using namespace ulapack;
using namespace libsnapper;
static int meshNum;
static double start_time = 0.0;
static double stop_time = numeric_limits<double>::max();
//
// Command-line arguments
//
static FILE *pts_cov_fp;
static string stereo_config_file_name;
static string contents_file_name;
static string dir_name;
static bool output_uv_file=false;
static bool use_undistorted_images = false;
static bool pause_after_each_frame = false;
static double image_scale = 1.0;
static int max_feature_count = 5000;

static bool have_max_frame_count = false;
static unsigned int max_frame_count;

static bool display_debug_images = true;
static bool output_pts_cov=false;
static bool use_sift_features = false;
static bool use_surf_features = false;
static bool use_ncc = false;
static int skip_counter=0;
static int num_skip=0;
static bool triangulate = false;
static string triangulation_file_name;
static bool have_cov_file=false;
static string stereo_calib_file_name;
static bool use_rect_images=false;
static FILE *uv_fp;
static ofstream file_name_list;
static FILE *bboxfp;
static double feature_depth_guess = AUV_NO_Z_GUESS;

static FILE *fpp,*fpp2;
static bool use_dense_feature=false;
//StereoMatching* stereo;
//StereoImage simage;
static int mesh_count=0;
static bool output_ply_and_conf =false;
static FILE *conf_ply_file;
static bool output_3ds=false;
static char cov_file_name[255];
static 
void print_uv_3dpts( list<Feature*>          &features,
		    list<Stereo_Feature_Estimate> &feature_positions,
		    unsigned int                   left_frame_id,
		     unsigned int                   right_frame_id,
		     double timestamp, string leftname,string rightname);
void get_cov_mat(ifstream *cov_file,Matrix &mat){
 
  for(int i=0; i < 0; i ++)
    for(int j=0; j < 0; j ++)
      (*cov_file) >>   mat(i,j);

  

}
bool get_camera_params( Config_File *config_file, Vector &camera_pose )
{

 double tmp_pose[6];

   if( config_file->get_value( "STEREO_POSE_X", tmp_pose[AUV_POSE_INDEX_X]) == 0 )
   {
      return false;
   }
   if( config_file->get_value( "STEREO_POSE_Y", tmp_pose[AUV_POSE_INDEX_Y]) == 0 )
   {
      return false;
   }
   if( config_file->get_value( "STEREO_POSE_Z", tmp_pose[AUV_POSE_INDEX_Z]) == 0 )
   {
      return false;
   }
   if( config_file->get_value( "STEREO_POSE_PHI", tmp_pose[AUV_POSE_INDEX_PHI]) == 0 )
   {
      return false;
   }
   if( config_file->get_value( "STEREO_POSE_THETA", tmp_pose[AUV_POSE_INDEX_THETA]) == 0 )
   {
      return false;
   }
   if( config_file->get_value( "STEREO_POSE_PSI", tmp_pose[AUV_POSE_INDEX_PSI]) == 0 )
   {
      return false;
   }
   
   camera_pose[AUV_POSE_INDEX_X]=tmp_pose[AUV_POSE_INDEX_X];
   camera_pose[AUV_POSE_INDEX_Y]=tmp_pose[AUV_POSE_INDEX_Y];
   camera_pose[AUV_POSE_INDEX_Z]=tmp_pose[AUV_POSE_INDEX_Z];
   camera_pose[AUV_POSE_INDEX_PHI]=tmp_pose[AUV_POSE_INDEX_PHI];
   camera_pose[AUV_POSE_INDEX_THETA]=tmp_pose[AUV_POSE_INDEX_THETA];
   camera_pose[AUV_POSE_INDEX_PSI]=tmp_pose[AUV_POSE_INDEX_PSI];

   return true;
}
//
// Parse command line arguments into global variables
//
static bool parse_args( int argc, char *argv[ ] )
{
   bool have_stereo_config_file_name = false;
   bool have_contents_file_name = false;
   
   int i=1;
   while( i < argc )
   {
      if( strcmp( argv[i], "-r" ) == 0 )
      {
         if( i == argc-1 ) return false;
         image_scale = strtod( argv[i+1], NULL );
         i+=2;
      }
      else if( strcmp( argv[i], "-m" ) == 0 )
      {
         if( i == argc-1 ) return false;
         max_feature_count = atoi( argv[i+1] );
         i+=2;
      }  
      else if( strcmp( argv[i], "-f" ) == 0 )
      {
         if( i == argc-1 ) return false;
         dir_name=string( argv[i+1]) ;
         i+=2;
      }
      else if( strcmp( argv[i], "-z" ) == 0 )
      {
         if( i == argc-1 ) return false;
         feature_depth_guess = atof( argv[i+1] );
         i+=2;
      }
      else if( strcmp( argv[i], "-s" ) == 0 )
      {
         if( i == argc-1 ) return false;
         num_skip = atoi( argv[i+1] );
         skip_counter=num_skip;
	 i+=2;
      }
      else if( strcmp( argv[i], "--cov" ) == 0 )
      {
         if( i == argc-1 ) return false;
         strcpy(cov_file_name, argv[i+1] );
	 have_cov_file=true;
	 i+=2;
      }
      else if( strcmp( argv[i], "-n" ) == 0 )
      {
         if( i == argc-1 ) return false;
         have_max_frame_count = true;
         max_frame_count = atoi( argv[i+1] );
         i+=2;
      }
      else if( strcmp( argv[i], "-t" ) == 0 )
      {
         if( i == argc-1 ) return false;
         triangulate = true;
         triangulation_file_name = argv[i+1];
         i+=2;
      }
      else if( strcmp( argv[i], "-u" ) == 0 )
      {
         use_undistorted_images = true;
         i+=1;
      }
      else if( strcmp( argv[i], "--ptscov" ) == 0 )
      {
         output_pts_cov = true;
         i+=1;
      }
      else if( strcmp( argv[i], "-y" ) == 0 )
      {
         use_rect_images = true;
         i+=1;
      }
      else if( strcmp( argv[i], "-d" ) == 0 )
      {
         display_debug_images = false;
         i+=1;
      }
      else if( strcmp( argv[i], "-p" ) == 0 )
      {
         pause_after_each_frame = true;
         i+=1;
      }
      else if( strcmp( argv[i], "-c" ) == 0 )
      {
         use_ncc = true;
         i+=1;
      }
      else if( strcmp( argv[i], "--confply" ) == 0 )
      {
         output_ply_and_conf = true;
         i+=1;
      }
      else if( strcmp( argv[i], "--uv" ) == 0 )
      {
         output_uv_file = true;
         i+=1;
      }
      else if( strcmp( argv[i], "--sift" ) == 0 )
      {
         use_sift_features = true;
         i+=1;
      }
      else if( strcmp( argv[i], "--dense-features" ) == 0 )
      {
         use_dense_feature = true;
         i+=1;
      }
      else if( strcmp( argv[i], "--surf" ) == 0 )
      {
         use_surf_features = true;
         i+=1;
      }
      else if( strcmp( argv[i], "--start" ) == 0 )
      {
         if( i == argc-1 ) return false;
         start_time = strtod( argv[i+1], NULL );
         i+=2;
      }
      else if( strcmp( argv[i], "--stop" ) == 0 )
      {
         if( i == argc-1 ) return false;
         stop_time = strtod( argv[i+1], NULL );
         i+=2;
      }
      else if( strcmp( argv[i], "--3ds" ) == 0 )
      {
	output_3ds=true;
	i+=1;
      }
      else if( !have_stereo_config_file_name )
      {
         stereo_config_file_name = argv[i];
         have_stereo_config_file_name = true;
         i++;
      }
      else if( !have_contents_file_name )
      {
         contents_file_name = argv[i];
         have_contents_file_name = true;
         i++;
      }
      else
      {
         cerr << "Error - unknown parameter: " << argv[i] << endl;
         return false;
      }
   }


#ifndef HAVE_LIBKEYPOINT
   if( use_sift_features || use_surf_features )
   {
      cerr << "ERROR - libsnapper was compiled without sift support" << endl;
      return false;
   }
#endif

   return (have_contents_file_name && have_stereo_config_file_name);
}


//
// Display information on how to use this program
//
static void print_usage( void )
{
   cout << "USAGE:" << endl;
   cout << "   stereo_feature_finder_test [OPTIONS] <stereo_cfg> <contents_file>" << endl; 
   cout << endl;
   cout << "OPTIONS:" << endl;
   cout << "   -r <resize_scale>       Resize the images by a scaling factor." << endl;
   cout << "   -m <max_feature_count>  Set the maximum number of features to be found." << endl;
   cout << "   -n <max_frame_count>    Set the maximum number of frames to be processed." << endl;
   cout << "   -z <feature_depth>      Set an estimate for the feature depth relative to cameras." << endl;
   cout << "   -t <output_file>        Save triangulate feature positions to a file" << endl;
 cout << "   --cov <file>               Input covar file." << endl; 
  cout << "   -c                      Use the normalised cross correlation feature descriptor" << endl;
   cout << "   --sift                  Find SIFT features." << endl;
   cout << "   --surf                  Find SURF features." << endl;
   cout << "   -d                      Do not display debug images." << endl;
   cout << "   --confply               Output confply file." << endl;

   cout << "   -p                      Pause after each frame." << endl;
   cout << "   --uv                    Output UV File." << endl;
   cout << "   --3ds                   Output 3ds Files." << endl;
   cout << "   --dense-features        Dense features ." << endl;
   cout << "   --ptscov                Output pts and cov ." << endl;

   cout << endl;
}



//
// Get the time in seconds (since the Unix Epoch in 1970)
//
static double get_time( void )
{
   struct timeval tv;
   
   gettimeofday( &tv, NULL );
   return tv.tv_sec+(double)tv.tv_usec/1e6;
}



//
// Remove radial and tangential distortion from the pair of stereo images
//
static void undistort_images( const Stereo_Calib  *calib,
                              const IplImage      *left_image,
                              const IplImage      *right_image,
                              IplImage           *&undist_left_image,
                              IplImage           *&undist_right_image )
{
   assert( calib != NULL );

   static Undistort_Data *left_undist_data = NULL;
   static Undistort_Data *right_undist_data = NULL;

   if( left_undist_data == NULL )
   {
      bool interpolate = false;
      left_undist_data = new Undistort_Data( calib->left_calib,
                                             left_image,
                                             interpolate );

      right_undist_data = new Undistort_Data( calib->right_calib,
                                              right_image,
                                              interpolate );
   }
   
   undist_left_image = cvCreateImage( cvGetSize(left_image),
                                      left_image->depth,
                                      left_image->nChannels );
   undist_right_image = cvCreateImage( cvGetSize(right_image),
                                       right_image->depth,
                                       right_image->nChannels );
   
   undistort_image( *left_undist_data, left_image, undist_left_image );
   undistort_image( *right_undist_data, right_image, undist_right_image );
} 


//
// Remove radial and tangential distortion from the pair of stereo images
//
static void rectify_images( const string &calib_file_name,
                               IplImage      *left_image,
			    IplImage      *right_image)/*,
                              IplImage           *&rect_left_image,
                              IplImage           *&rect_right_image )
						       */{
  // Get OpenCV to load the calibration file.
   CvCalibFilter opencv_calib;
   opencv_calib.LoadCameraParams( calib_file_name.c_str( ) );

 // Test to see load was successful
   if( opencv_calib.GetStereoParams( ) == NULL )
   {
      stringstream buf;
      buf << "Unable to load stereo config file: " << calib_file_name;
      throw buf.str( );
      std::cerr << "Unable to load stereo config file: " << calib_file_name;
   }
   
   IplImage* images[] = { left_image, right_image };
   
   if(opencv_calib.Undistort(images, images) == false){
     std::cerr << "Image Undistort failed" << std::endl;
   }
   if (opencv_calib.Rectify(images, images) == false ){
     std::cerr << "Image Rectification failed" << std::endl;
   }
   cvSaveImage("left.png",left_image);
   cvSaveImage("right.png",right_image);
     int maxdisp = 64;
     IplImage* depthImage = cvCreateImage(cvGetSize(left_image), 8, 1);
     cvFindStereoCorrespondence( left_image, right_image, CV_DISPARITY_BIRCHFIELD, depthImage, maxdisp);
     cvNamedWindow("depthImage",1);
     cvShowImage("depthImage", left_image);
     cvResizeWindow("depthImage", depthImage->width, depthImage->height);
     cvWaitKey( 0 );
} 

void update_bbox (GtsPoint * p, GtsBBox * bb)
{
  if (p->x < bb->x1) bb->x1 = p->x;
  if (p->y < bb->y1) bb->y1 = p->y;
  if (p->z < bb->z1) bb->z1 = p->z;
  if (p->x > bb->x2) bb->x2 = p->x;
  if (p->y > bb->y2) bb->y2 = p->y;
  if (p->z > bb->z2) bb->z2 = p->z;
}

void save_bbox_frame (GtsBBox * bb, FILE * fptr){
  g_return_if_fail (bb != NULL);

  fprintf (fptr, "%g %g %g %g %g %g\n",
	     bb->x1, bb->y1, bb->z1,
	     bb->x2, bb->y2, bb->z2);

}


//
// Load the next pair of images from the contents file
//
static bool get_stereo_pair( const string  &contents_dir_name,
                             ifstream      &contents_file,
                             Stereo_Calib *stereo_calib,
                             IplImage     *&left_image,
                             IplImage     *&right_image,
			     IplImage     *&color_image,
                             unsigned int  &left_frame_id,
                             unsigned int  &right_frame_id,
                             string        &left_image_name,
                             string        &right_image_name, 
			     double &x,double &y, double &z,
			     double &r,double &p, double &h,
			     double &timestamp)
{
   static unsigned int frame_id = 0;
   int index;
   //
   // Try to read timestamp and file names
   //
   bool readok;
   do{
    
     readok =(contents_file >> index &&
	 contents_file >> timestamp &&
         contents_file >> left_image_name &&
         contents_file >> right_image_name &&
	 contents_file >> x &&
	 contents_file >> y &&
	 contents_file >> z &&
	 contents_file >> r &&
	 contents_file >> p &&
	      contents_file >> h );
     
   }
   while (readok && (timestamp < start_time || (skip_counter++ < num_skip)));
   skip_counter=0;
   
   if(!readok || timestamp >= stop_time) {
     // we've reached the end of the contents file
     return false;
   }      
   
   
     
   //
   // Load the images (-1 for unchanged grey/rgb)
   //
   string complete_left_name( contents_dir_name+left_image_name );
   left_image  = cvLoadImage( complete_left_name.c_str( ) , -1 );
   if( left_image == NULL )
   {
      cerr << "ERROR - unable to load image: " << complete_left_name << endl;
      return false;
   }

   string complete_right_name( contents_dir_name+right_image_name );
   right_image = cvLoadImage( complete_right_name.c_str( ), -1 );
   if( right_image == NULL )
   {
      cerr << "ERROR - unable to load image: " << complete_right_name << endl;
      cvReleaseImage( &left_image );
      return false;
   }

   
   //
   // Convert to greyscale. Use cvCvtColor for consistency. Getting cvLoadImage to load
   // the images as greyscale may use different RGB->greyscale weights
   //
   if( left_image->nChannels == 3 )
   {
     color_image=left_image;
      IplImage *grey_left  = cvCreateImage( cvGetSize(left_image) , IPL_DEPTH_8U, 1 );
      cvCvtColor( left_image , grey_left , CV_BGR2GRAY );
      // IplImage *temp = left_image;
      left_image = grey_left;
      //cvReleaseImage( &temp );
   }

   if( left_image->nChannels == 3 )
   {
      IplImage *grey_right = cvCreateImage( cvGetSize(right_image), IPL_DEPTH_8U, 1 );
      cvCvtColor( right_image, grey_right, CV_BGR2GRAY );
      IplImage *temp = right_image;
      right_image = grey_right;
      cvReleaseImage( &temp );
   }

 
   //
   // Undistrort images if required
   // 
   if( use_undistorted_images )
   {
      IplImage *undist_left_image;
      IplImage *undist_right_image;

      undistort_images( stereo_calib, left_image, right_image,
                        undist_left_image, undist_right_image );

      cvReleaseImage( &left_image );
      cvReleaseImage( &right_image );
      left_image = undist_left_image;
      right_image = undist_right_image;
   }

 
   //
   // Rect images if required
   // 
   if( use_rect_images && !use_undistorted_images)
   {
     /*IplImage *rect_left_image;
      IplImage *rect_right_image;
     */
     printf("REctiftiny\n");
      
      rectify_images(stereo_calib_file_name, left_image, right_image);

      cvReleaseImage( &left_image );
      cvReleaseImage( &right_image );
      /*left_image = undist_left_image;
	right_image = undist_right_image;*/
   }

   //
   // Scale images if required
   // 
   if( image_scale != 1.0 )
   {
      CvSize scaled_size = cvGetSize( left_image );
      scaled_size.width  = (int)(scaled_size.width*image_scale  );
      scaled_size.height = (int)(scaled_size.height*image_scale );

      IplImage *scaled_left  = cvCreateImage( scaled_size, 8, 1 );
      IplImage *scaled_right = cvCreateImage( scaled_size, 8, 1 );
      cvResize( left_image, scaled_left );
      cvResize( right_image, scaled_right );

      cvReleaseImage( &left_image );
      cvReleaseImage( &right_image );
      left_image = scaled_left;
      right_image = scaled_right;
   }

   left_frame_id = frame_id++;
   right_frame_id = frame_id++;
   return true;
}       
                        

int main( int argc, char *argv[ ] )
{
   //
   // Parse command line arguments
   //
   if( !parse_args( argc, argv ) )
   {
      print_usage( );
      exit( 1 );
   }


   //
   // Figure out the directory that contains the config file 
   //
   string config_dir_name;
   int slash_pos = stereo_config_file_name.rfind( "/" );
   if( slash_pos != -1 )
      config_dir_name = stereo_config_file_name.substr( 0, slash_pos+1 );


   //
   // Open the config file
   // Ensure the option to display the feature finding debug images is on.
   //
   Config_File *config_file;
   try
   {
      config_file = new Config_File( stereo_config_file_name );
   }
   catch( string error )
   {
      cerr << "ERROR - " << error << endl;
      exit( 1 );
   }

   Config_File *dense_config_file;
   try
   {
     dense_config_file = new Config_File( "semi-dense.cfg");
   }
   catch( string error )
   {
      cerr << "ERROR - " << error << endl;
      exit( 1 );
   }

   config_file->set_value( "SKF_SHOW_DEBUG_IMAGES"    , display_debug_images );
   config_file->set_value( "SCF_SHOW_DEBUG_IMAGES"    , display_debug_images );
   config_file->set_value( "NCC_SCF_SHOW_DEBUG_IMAGES", display_debug_images );

   if( use_sift_features )
      config_file->set_value( "SKF_KEYPOINT_TYPE", "SIFT" );
   else if( use_surf_features )   
      config_file->set_value( "SKF_KEYPOINT_TYPE", "SURF" );


   //
   // Load the stereo camera calibration 
   //
   Stereo_Calib *calib = NULL;
   bool have_stereo_calib = false;
   
   if( config_file->get_value( "STEREO_CALIB_FILE", stereo_calib_file_name) )
   {
      stereo_calib_file_name = config_dir_name+stereo_calib_file_name;
      try
      {
         calib = new Stereo_Calib( stereo_calib_file_name );
      }
      catch( string error )
      {
         cerr << "ERROR - " << error << endl;
         exit( 1 );
      }
      have_stereo_calib = true;
   }      

   if( use_undistorted_images == true && have_stereo_calib == false )
   {
      cerr << "ERROR - A stereo calibration file is required to undistort "
           << "images." << endl;
      exit( 1 );     
   }
 
   //
   // Open the contents file
   //
   ifstream contents_file( contents_file_name.c_str( ) );
   if( !contents_file )
   {
      cerr << "ERROR - unable to open contents file: " << contents_file_name
           << endl;
      exit( 1 );     
   }
   
 
     
   ifstream *cov_file = new ifstream;
   if(have_cov_file){ 
     cov_file->open( cov_file_name);
       if( !cov_file )
	 {
	   cerr << "ERROR - unable to open contents file: " << cov_file_name
		<< endl;
	   exit( 1 );     
	 }
   }

   if(output_ply_and_conf){
     const char *uname="mesh-agg";
     auv_data_tools::makedir(uname);
     conf_ply_file=fopen("mesh-agg/surface.conf","w");
     bboxfp = fopen("mesh-agg/bbox.txt","w");
   }
   if(output_pts_cov)
     pts_cov_fp=  fopen("pts_cov.txt","w");

   //
   // Figure out the directory that contains the contents file 
   //
   
   
   
   if(output_uv_file)
     uv_fp= fopen("uvfile.txt","w");
   if(output_3ds){
     const char *uname="mesh";
     auv_data_tools::makedir(uname); 
     file_name_list.open("mesh/filenames.txt");
   }  
 //
   // Create the stereo feature finder
   //
   Stereo_Feature_Finder *finder = NULL;
   Stereo_Feature_Finder *finder_dense = NULL;
   if( use_sift_features || use_surf_features )
   {
#ifdef HAVE_LIBKEYPOINT
      finder = new Stereo_Keypoint_Finder( *config_file, 
                                            use_undistorted_images, 
                                            image_scale, 
                                            calib );
#endif
   }
   else if( use_ncc )
   {
      finder = new Stereo_NCC_Corner_Finder( *config_file, 
                                              use_undistorted_images, 
                                              image_scale, 
                                              calib );
   }
   else
   {
      finder = new Stereo_Corner_Finder( *config_file, 
                                         use_undistorted_images, 
                                         image_scale, 
                                         calib );
      if(use_dense_feature)
	finder_dense = new Stereo_Corner_Finder( *dense_config_file, 
                                         use_undistorted_images, 
                                         image_scale, 
                                         calib );
   }
   
         
   //
   // Run through the data
   //
   IplImage *left_frame;
   IplImage *right_frame;
   IplImage *color_frame;
   unsigned int left_frame_id;
   unsigned int right_frame_id;
   string left_frame_name;
   string right_frame_name;
   unsigned int stereo_pair_count = 0;
   Matrix *image_coord_covar;
   Vector camera_pose(AUV_NUM_POSE_STATES);
   get_camera_params(config_file,camera_pose);

   if(have_cov_file){
     image_coord_covar = new Matrix(4,4);
     image_coord_covar->clear( );
     config_file->get_value("STEREO_LEFT_X_VAR",(*image_coord_covar)(0,0) );
     config_file->get_value("STEREO_LEFT_Y_VAR",(*image_coord_covar)(1,1) );
     config_file->get_value("STEREO_RIGHT_X_VAR",(*image_coord_covar)(2,2));
     config_file->get_value("STEREO_RIGHT_Y_VAR",(*image_coord_covar)(3,3));
   }else
     image_coord_covar=NULL;
   
  
   
   
   while( !have_max_frame_count || stereo_pair_count < max_frame_count ){
      //
      // Load the images
      //
     double timestamp;
     printf("Loading images %d\n",stereo_pair_count);
      double load_start_time = get_time( );
      Vector veh_pose(AUV_NUM_POSE_STATES);
      if( !get_stereo_pair( dir_name, contents_file, calib,
                            left_frame, right_frame,
                            color_frame,left_frame_id, right_frame_id,
                            left_frame_name, right_frame_name,
			    veh_pose[AUV_POSE_INDEX_X],
			    veh_pose[AUV_POSE_INDEX_Y],
			    veh_pose[AUV_POSE_INDEX_Z],
			    veh_pose[AUV_POSE_INDEX_PHI],
			    veh_pose[AUV_POSE_INDEX_THETA],
			    veh_pose[AUV_POSE_INDEX_PSI],
			    timestamp ))
      {
         break;
      }                            
      double load_end_time = get_time( );
      
      
    

      if(output_3ds)
	file_name_list << dir_name+left_frame_name  << " " << dir_name+right_frame_name << endl;

      //
      // Find the features
      //
      cout << "Finding features..." << endl;
      double find_start_time = get_time( );
      list<Feature *> features;
      finder->find( left_frame,
                    right_frame,
                    left_frame_id,
                    right_frame_id,
		    max_feature_count,
                    features,
                    feature_depth_guess );
      if(use_dense_feature) 
	finder_dense->find( left_frame,
                    right_frame,
                    left_frame_id,
                    right_frame_id,
		    max_feature_count,
                    features,
                    feature_depth_guess );
      
      double find_end_time = get_time( );
      
      
      //
      // Triangulate the features if requested
      //

	if(!fpp2 && output_3ds)
	  fpp2=fopen("mesh/vehpath.txt","w");
	if(!fpp && output_3ds)	
	  fpp=fopen("mesh/campath.txt","w");

	
         cout << "Saving triangulation to file: "
              << triangulation_file_name << endl;
         
         Stereo_Reference_Frame ref_frame = STEREO_LEFT_CAMERA;
       


	 Matrix pose_cov(4,4);
	 get_cov_mat(cov_file,pose_cov);

         //cout << "Cov " << pose_cov << "Pose "<< veh_pose<<endl;
	 list<Stereo_Feature_Estimate> feature_positions;
         stereo_triangulate( *calib,
                             ref_frame,
                             features,
                             left_frame_id,
                             right_frame_id,
                             image_coord_covar,
                             feature_positions );
	
	 //   static ofstream out_file( triangulation_file_name.c_str( ) );
	 if(output_uv_file)
	   print_uv_3dpts(features,feature_positions,
			  left_frame_id,right_frame_id,timestamp,
			  left_frame_name,right_frame_name);
	   
	  static Vector stereo1_nav( AUV_NUM_POSE_STATES );
	 // Estimates of the stereo poses in the navigation frame


	 list<Stereo_Feature_Estimate>::iterator litr;
	 GPtrArray *localV = g_ptr_array_new ();
	 GtsRange r;
	 gts_range_init(&r);
	 TVertex *vert;
         for( litr  = feature_positions.begin( ) ;
              litr != feature_positions.end( ) ;
              litr++ )
         {
	  
	   vert=(TVertex*)  gts_vertex_new (t_vertex_class (),
					    litr->x[0],litr->x[1],litr->x[2]);
	   double confidence=1.0;
	   Vector max_eig_v(3);
	   if(have_cov_file){
	     
	     Matrix eig_vecs( litr->P );
	     Vector eig_vals(3);
	     int work_size = eig_sym_get_work_size( eig_vecs, eig_vals );
	     
	     Vector work( work_size );
	     eig_sym_inplace( eig_vecs, eig_vals, work );
	     /* cout << "  eig_vecs: " << eig_vecs << endl;
	     cout << "  eig_vals: " << eig_vals << endl;
	     */
	     double maxE=DBL_MIN;
	     int maxEidx=0;
	     for(int i=0; i < 3; i++){
	       if(eig_vals[i] > maxE){
		 maxE=eig_vals[i];
		 maxEidx=i;
	       }
	     }

	     for(int i=0; i<3; i++)
	       max_eig_v(i)=eig_vecs(i,maxEidx);
	         
	     confidence= 2*sqrt(maxE);
	     max_eig_v = max_eig_v / sum(max_eig_v);
	     //    cout << "  eig_ max: " << max_eig_v << endl;
	     
	   
	   }
	   vert->confidence=confidence;
	   vert->ex=max_eig_v(0);
	   vert->ey=max_eig_v(1);
	   vert->ez=max_eig_v(2);
	   gts_range_add_value(&r,vert->confidence);
	   g_ptr_array_add(localV,GTS_VERTEX(vert));
	   if(output_pts_cov){
	     // cout << litr->P << endl;
	     fprintf(pts_cov_fp,"%f %f %f ",litr->x[0],litr->x[1],litr->x[2]);
	     for(int i=0;  i<3; i++)
	       for(int j=0;  j<3; j++)

		 fprintf(pts_cov_fp,"%g ",litr->P(i,j));
	     fprintf(pts_cov_fp,"\n");
	   }

	 }
	 gts_range_update(&r);
	 for(unsigned int i=0; i < localV->len; i++){
	   TVertex *v=(TVertex *) g_ptr_array_index(localV,i);
	   if(have_cov_file){
	     float val= (v->confidence -r.min) /(r.max-r.min);
	     jet_color_map(val,v->r,v->g,v->b);
	   }
	 }
	 

	 GtsSurface *surf= auv_mesh_pts(localV,2.0,0); 
	 if(output_3ds){
	   //meshGen->createTexture(color_frame);  
	   //meshGen->GenTexCoord(surf,&calib->left_calib);
	 }
	 GtsMatrix *m=get_sensor_to_world_trans(veh_pose,camera_pose);
	 gts_surface_foreach_vertex (surf, (GtsFunc) gts_point_transform, m);
	 gts_matrix_destroy (m);
	 
	 if(output_ply_and_conf){
	   
	   GtsBBox *bbox=gts_bbox_surface(gts_bbox_class(),surf);

	   fprintf(bboxfp,"%d %f %f %f %f %f %f\n",stereo_pair_count,
		   bbox->x1,bbox->y1,bbox->z1,
		   bbox->x2,bbox->y2,bbox->z2); 
	 
	   
	   char filename[255];
	   FILE *fp;
	   sprintf(filename,"mesh-agg/surface-%04d.ply",
		   mesh_count++);
	   fp = fopen(filename, "w" );
	   auv_write_ply(surf, fp,have_cov_file,"test");
	   fclose(fp);
	   fprintf(conf_ply_file,
		   "bmesh surface-%04d.ply\n"
		   ,mesh_count-1);
	 }
	 /* if(output_3ds){
	   
	   
	 
	   
	 
	   
	   

	   meshGen->aggSurf.push_back(surf); 
	   meshNum= meshGen->meshNum;
	   
	   
	   if((meshGen->aggSurf.size() % cfg.meshSplit) == 0){
	     printf("Creating mesh output\n");
	     meshGen->createFinalMesh();
	   }
	   
	   fprintf(fpp2,"%f %f %f %f %f %f %f %f %f %f\n",   
		   timestamp,
		   veh_pose[AUV_POSE_INDEX_X],
		   veh_pose[AUV_POSE_INDEX_Y],
		   veh_pose[AUV_POSE_INDEX_Z],
		   veh_pose[AUV_POSE_INDEX_PHI],
		   veh_pose[AUV_POSE_INDEX_THETA],
		   fmod(veh_pose[AUV_POSE_INDEX_PSI],(M_PI)),
		   0.0,0.0,0.0);
	   
	   fprintf(fpp,"%f %f %f %f %f %f %f\n",   
		   timestamp,
		   veh_pose[AUV_POSE_INDEX_X],
		   veh_pose[AUV_POSE_INDEX_Y],
		   veh_pose[AUV_POSE_INDEX_Z],
		   veh_pose[AUV_POSE_INDEX_PHI],
		   veh_pose[AUV_POSE_INDEX_THETA],
		   fmod(veh_pose[AUV_POSE_INDEX_PSI],(M_PI))
		   );
	 }
	 */
  
	
   
      //
      // Display useful info 
      //
      cout << endl;
      cout << "Left Image : " << left_frame_name << endl;
      cout << "Right Image: " << right_frame_name << endl;
      cout << endl;
      cout << "Number of features found: " << features.size( ) << endl;
      cout << endl;
      cout << "Image loading time     : " << load_end_time-load_start_time << endl;
      cout << "Feature finding time   : " << find_end_time-find_start_time << endl;
      cout << endl;
      cout << "------------------------------------" << endl;
      cout << endl;


      //
      // Pause between frames if requested.
      //
      if( display_debug_images && pause_after_each_frame )
         cvWaitKey( 0 );
      else if( display_debug_images )
         cvWaitKey( 100 );
                  
      
      //
      // Clean-up
      //
      cvReleaseImage( &left_frame );
      cvReleaseImage( &right_frame );
      cvReleaseImage( &color_frame);
      list<Feature*>::iterator fitr;
      for( fitr  = features.begin( ) ;
           fitr != features.end( ) ;
           fitr++ )
      {
         delete *fitr;
      }     

      stereo_pair_count++;
   }
  

   /*if(meshGen->aggSurf.size() >= 0){
	   printf("Creating mesh output\n");
	   meshGen->createFinalMesh();
	   }*/

   if(output_uv_file)
     fclose(uv_fp);
   if(output_3ds)
     file_name_list.close();
   
   if(fpp)
     fclose(fpp);
   if(output_3ds){
     fpp = fopen("mesh/meshinfo.txt","w");
     fprintf(fpp,"%d\n",meshNum);
     fclose(fpp);
   }
   if(fpp2)
     fclose(fpp2);
   if(conf_ply_file){
    fclose(conf_ply_file);
    if(output_pts_cov)
      fclose(pts_cov_fp);
   
 
    conf_ply_file=fopen("runvrip.sh","w+");
    //fprintf(conf_ply_file,"#!/bin/bash\nPATH=$PATH:$PWD/myvrip/bin/\ncd mesh-agg/ \n../myvrip/bin/vripnew auto.vri surface.conf surface.conf 0.033 -prob\n../myvrip/bin/vripsurf auto.vri out.ply -import_norm\n");
fprintf(conf_ply_file,"#!/bin/bash\nPATH=$PATH:$PWD/myvrip/bin/\ncd mesh-agg/ \n../myvrip/bin/vripnew auto.vri surface.conf surface.conf 0.033 -rampscale 100\n../myvrip/bin/vripsurf auto.vri out.ply\n");
    fchmod(fileno(conf_ply_file),   0777);
 fclose(conf_ply_file);
   
    
    
    system("./runvrip.sh");
   }
   // 
   // Clean-up
   //
   delete config_file;
   delete calib;
   delete finder;
   if(use_dense_feature)
     delete finder_dense;
   return 0;
}

void print_uv_3dpts( list<Feature*>          &features,
		    list<Stereo_Feature_Estimate> &feature_positions,
		    unsigned int                   left_frame_id,
		     unsigned int                   right_frame_id,
		     double timestamp, string leftname,string rightname){
  list<Stereo_Feature_Estimate>::iterator litr;
  list<Feature *>::iterator fitr;
  fprintf(uv_fp,"%f %s %s %d %d\n",timestamp, leftname.c_str(),
	  rightname.c_str(),(int)feature_positions.size(),(int)features.size());
  
  for( litr  = feature_positions.begin( ),
	 fitr = features.begin();
       litr != feature_positions.end( ) || fitr != features.end(); 
       litr++,fitr++ ){
    const Feature_Obs *p1_left_obs;
    p1_left_obs = (*fitr)->get_observation( left_frame_id );
    const Feature_Obs *p1_right_obs;
    p1_right_obs = (*fitr)->get_observation( right_frame_id );
    fprintf(uv_fp,"%f %f %f %f %f %f %f\n",litr->x[0],litr->x[1],
	   litr->x[2], p1_left_obs->u, 
	   p1_left_obs->v,
	   p1_right_obs->u, 
	   p1_right_obs->v);
    
  } 
  fprintf(uv_fp,"\n");
}
