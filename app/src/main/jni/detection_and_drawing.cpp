#include <jni.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include "opencv2/features2d.hpp"
#include "opencv2/core/affine.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include <vector>
#include <sstream>
#include <android/log.h>
#include "aruco.hpp"

#define APPNAME "CardboardKeyboard"
#define SORTED_IDS_SIZE 17
#define TOP_LEFT 0
#define TOP_RIGHT 1
#define BOTTOM_RIGHT 2
#define BOTTOM_LEFT 3
#define KEYS_COUNT 49

using namespace std;
using namespace cv;
using namespace aruco;

enum Color {COLOR_C, COLOR_D, COLOR_E, COLOR_F, COLOR_G, COLOR_A, COLOR_H, COLOR_TEXT};
enum OctaveNote {C, D, E, F, G, A, H, CC};

extern "C" {

// Structure of starting and ending points of lines drawn on keys belonging to a chord.
struct ChordLinesPoints {
    Point2f lineStarts[3];
    Point2f lineEnds[3];
};

// Conversion function because NDK doesn't support to_string.
string intToString(int num)
{
    ostringstream convert;
    convert << num;
    return convert.str();
}

// Return vector which indexes are markerIds from 1 to n and its values are indexes of markerCorners.
// Example: marker with id 4 was detected first, so sortedIds[4] == 0
// @param &markerIds Reference to vector of marker ID's.
// @return Vector of marker ID's sorted by id.
vector<int> getSortedIds(vector<int> &markerIds) {
    // Initialize vector with fixed size and values -1.
    vector<int> sortedIds(SORTED_IDS_SIZE, -1);
    int i = 0;
    int minId = SORTED_IDS_SIZE; // Lowest markerId, needed to determine octave.

    for(vector<int>::iterator it = markerIds.begin(); it != markerIds.end(); it++) {
        // Put markerId index as value and markerId as index to sortedIds.
        sortedIds.at(*it) = i;
        ++i;
        if(*it < minId) minId = *it;
    }
    // Erase all unused elements(value == -1).
    sortedIds.erase(remove(sortedIds.begin(), sortedIds.end(), -1), sortedIds.end());

    // Store minimal ID for determining octave.
    sortedIds.push_back(minId);

    return sortedIds;
}

Scalar getColor(Color c) {
    static Scalar colors[8];
    colors[static_cast<int>(COLOR_TEXT)] = Scalar(0, 210, 0);
    colors[static_cast<int>(COLOR_C)] = Scalar(239, 10, 0);
    colors[static_cast<int>(COLOR_D)] = Scalar(0, 14, 239);
    colors[static_cast<int>(COLOR_E)] = Scalar(250, 90, 7);
    colors[static_cast<int>(COLOR_F)] = Scalar(240, 0, 230);
    colors[static_cast<int>(COLOR_G)] = Scalar(240, 240, 0);
    colors[static_cast<int>(COLOR_A)] = Scalar(117, 44, 0);
    colors[static_cast<int>(COLOR_H)] = Scalar(0, 230, 240);

    return colors[c];
}

// Return octave number for given marker and count of keys.
// The formula is generated by polynomial interpolation from pairs of values [marker ID, octave number].
// @param id ID of the marker which has the least ID of detected markers.
// @param keysCount Count of keys on the piano.
// @return Number of the octave.
int getOctaveNumber(int id, int keysCount) {
    switch(keysCount) {
        case 49:
        case 61:
        case 76:
            return (id + 3) / 2;
        case 88:
            return (id + 1) / 2;
        default: return (id + 3) / 2;
    }
}

// Draw note name with octave numbe on each key.
// @param &img Reference to overlay image.
// @param octave Number of octave.
void drawNoteNames(Mat &overlay, int octaveNumber) {
    int fontFace = FONT_HERSHEY_SIMPLEX;
    double fontScale = 2.0;
    int thickness = 3;

    double horizontalEighth = overlay.cols / 8;
    double verticalEighth = overlay.rows / 8;

    Point2f notePosition = Point2f((horizontalEighth / 8), (overlay.rows - verticalEighth));
    string notes("CDEFGAHC");

    for(auto c : notes) {
        putText(overlay, c + intToString(octaveNumber), notePosition, fontFace, fontScale, getColor(COLOR_TEXT), thickness);
        notePosition.x += horizontalEighth;
        // Second C is one octave higher.
        if(c == 'H') ++octaveNumber;
    }
}

// Return X coordinate of a given note key.
// @param horizontalEighth Eighth of a count of columns in the image.
// @return X coordinate of a given note key.
double getXCoordOfNote(OctaveNote note, double horizontalEighth) {
    switch(note) {
        case C:
            return 0.0;
        case D:
            return horizontalEighth;
        case E:
            return horizontalEighth * 2;
        case F:
            return horizontalEighth * 3;
        case G:
            return horizontalEighth * 4;
        case A:
            return horizontalEighth * 5;
        case H:
            return horizontalEighth * 6;
        case CC:
            return horizontalEighth * 7;
        default: break;
    }
}

// Return structure with starting and ending points of lines for a chord.
// @param chord Enum representing chord.
// @param horizontalEighth Eighth of a count of columns in the image.
// @param verticalEighth Eighth of a count of rows in the image.
// @return Structure with line points for a given chord.
ChordLinesPoints getChordLinePoints(OctaveNote chord, double horizontalEighth, double verticalEighth) {
    ChordLinesPoints linesPoints;
    Point2f point;

    switch(chord) {
        case C:
            point.y = verticalEighth * 5.5;
            point.x = getXCoordOfNote(C, horizontalEighth);
            linesPoints.lineStarts[0] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[0] = point;

            point.x = getXCoordOfNote(E, horizontalEighth);
            linesPoints.lineStarts[1] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[1] = point;

            point.x = getXCoordOfNote(G, horizontalEighth);
            linesPoints.lineStarts[2] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[2] = point;
            break;
        case D:
            point.y = verticalEighth * 5.6;
            point.x = getXCoordOfNote(D, horizontalEighth);
            linesPoints.lineStarts[0] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[0] = point;

            point.x = getXCoordOfNote(F, horizontalEighth);
            linesPoints.lineStarts[1] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[1] = point;

            point.x = getXCoordOfNote(A, horizontalEighth);
            linesPoints.lineStarts[2] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[2] = point;
            break;
        case E:
            point.y = verticalEighth * 5.7;
            point.x = getXCoordOfNote(E, horizontalEighth);
            linesPoints.lineStarts[0] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[0] = point;

            point.x = getXCoordOfNote(G, horizontalEighth);
            linesPoints.lineStarts[1] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[1] = point;

            point.x = getXCoordOfNote(H, horizontalEighth);
            linesPoints.lineStarts[2] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[2] = point;
            break;
        case F:
            point.y = verticalEighth * 5.8;
            point.x = getXCoordOfNote(F, horizontalEighth);
            linesPoints.lineStarts[0] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[0] = point;

            point.x = getXCoordOfNote(A, horizontalEighth);
            linesPoints.lineStarts[1] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[1] = point;

            point.x = getXCoordOfNote(CC, horizontalEighth);
            linesPoints.lineStarts[2] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[2] = point;
            break;
        case G:
            point.y = verticalEighth * 5.9;
            point.x = getXCoordOfNote(G, horizontalEighth);
            linesPoints.lineStarts[0] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[0] = point;

            point.x = getXCoordOfNote(H, horizontalEighth);
            linesPoints.lineStarts[1] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[1] = point;

            point.x = getXCoordOfNote(D, horizontalEighth);
            linesPoints.lineStarts[2] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[2] = point;
            break;
        case A:
            point.y = verticalEighth * 6.0;
            point.x = getXCoordOfNote(A, horizontalEighth);
            linesPoints.lineStarts[0] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[0] = point;

            point.x = getXCoordOfNote(E, horizontalEighth);
            linesPoints.lineStarts[1] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[1] = point;

            point.x = getXCoordOfNote(CC, horizontalEighth);
            linesPoints.lineStarts[2] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[2] = point;
            break;
        case H:
            point.y = verticalEighth * 6.1;
            point.x = getXCoordOfNote(H, horizontalEighth);
            linesPoints.lineStarts[0] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[0] = point;

            point.x = getXCoordOfNote(F, horizontalEighth);
            linesPoints.lineStarts[1] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[1] = point;

            point.x = getXCoordOfNote(D, horizontalEighth);
            linesPoints.lineStarts[2] = point;
            point.x += horizontalEighth;
            linesPoints.lineEnds[2] = point;
            break;
        default: break;
    }

    return linesPoints;
}

// Draw chord letters and lines to corresponding keys for chord.
// @param &octaveRoi Reference to overlay part which will be drawn to octave region.
// @param &wholeScreen Reference to image from camera.
void drawChords(Mat &overlay, Mat &wholeScreen) {
    int fontFace = FONT_HERSHEY_SIMPLEX;
    double fontScale = 1.4;
    int textThickness = 5;

    double horizontalEighth = overlay.cols / 8;
    double verticalEighth = overlay.rows / 8;

    string chordNames("CDEFGAH");
    // Chord names will be on top of the whole screen.
    Point2f namePosition = Point2f(horizontalEighth, verticalEighth);

    int lineThickness = 3;
    // Starting and ending points of lines to be drawn on keys belonging to chord.
    ChordLinesPoints linesPoints;

    // Iterate over chord names, draw a name on top of the screen and lines to keys.
    for(unsigned int i = 0; i < chordNames.size(); ++i) {
        putText(wholeScreen, chordNames.substr(i, 1), namePosition, fontFace, fontScale, getColor(static_cast<Color>(i)), textThickness);
        namePosition.x += horizontalEighth;

        linesPoints = getChordLinePoints(static_cast<OctaveNote>(i), horizontalEighth, verticalEighth);
        for(unsigned int j = 0; j < 3; ++j) {
            line(overlay, linesPoints.lineStarts[j], linesPoints.lineEnds[j], getColor(static_cast<Color>(i)), lineThickness);
            // Emphasize root note with white circle in the center of the line.
            if(j == 0) {
                Point2f point = Point2f((linesPoints.lineStarts[j].x + linesPoints.lineEnds[j].x) / 2, linesPoints.lineStarts[j].y);
                circle(overlay, point, 5, Scalar(255, 255, 255), -1);
            }
        }
    }

}

// Draw all virtual content to image.
// @param &mRgb Reference to color image from camera.
// @param &markerCorners Reference to vector of vectors of marker corners.
// @param sortedIds Vector of marker ID's sorted by ID.
void draw(Mat &mRgb, vector< vector<Point2f> > &markerCorners, vector<int> sortedIds) {
    // Overlay matrix, where all virtual content is drawn.
    Mat overlay(mRgb.rows, mRgb.cols, CV_8UC4);

    // Helper matrices.
    Mat overlayWarped, mask, maskInv, result1, result2;

    // Homography matrix.
    Mat H;

    // 4 corners of the octave in image from camera.
    vector< Point2f > octaveCorners;

    // 4 corners of the overlay image.
    vector< Point2f > overlayCorners;

    // Number of the octave.
    int octaveNumber = getOctaveNumber(sortedIds.back(), KEYS_COUNT);

    // Fill overlay corners.
    overlayCorners.push_back(Point2f(0.0, 0.0));
    overlayCorners.push_back(Point2f(0.0, overlay.rows));
    overlayCorners.push_back(Point2f(overlay.cols, 0.0));
    overlayCorners.push_back(Point2f(overlay.cols, overlay.rows));

    // Repeat for each octave, octaves share 2 markers on start/end.
    for(unsigned int i = 0; i < (sortedIds.size() - 3); i += 2)
    {
        drawNoteNames(overlay, octaveNumber);
        ++octaveNumber;

        drawChords(overlay, mRgb);

        // Fill octave corners.
        octaveCorners.push_back(markerCorners[sortedIds[i]][BOTTOM_LEFT]);
        octaveCorners.push_back(markerCorners[sortedIds[i+1]][BOTTOM_LEFT]);
        octaveCorners.push_back(markerCorners[sortedIds[i+2]][BOTTOM_RIGHT]);
        octaveCorners.push_back(markerCorners[sortedIds[i+3]][BOTTOM_RIGHT]);

        // Compute homography between overlay and octave corners.
        H = findHomography(overlayCorners, octaveCorners, RHO);
        octaveCorners.clear();

        if(H.empty()) {
            __android_log_print(ANDROID_LOG_VERBOSE, APPNAME, "ERR: findHomography returned empty matrix");
            return;
        }

        // Apply perspective transformation to overlay image according to computed homography.
        warpPerspective(overlay, overlayWarped, H, mRgb.size());

        // Create grayscale mask from warped image.
        cvtColor(overlayWarped, mask, CV_BGR2GRAY);

        // Make all drawn things white.
        threshold(mask, mask, 0, 255, CV_THRESH_BINARY);

        // Create an inversed mask.
        bitwise_not(mask, maskInv);

        // Mask image from camera.
        mRgb.copyTo(result1, maskInv);

        // Mask image with virtual content.
        overlayWarped.copyTo(result2, mask);

        // Create final image by adding two result matrices.
        mRgb = result1 + result2;

    }
}

JNIEXPORT void JNICALL
Java_cz_email_michalchomo_cardboardkeyboard_MainActivity_detectMarkersAndDraw(JNIEnv *env,
                                                                            jlong matAddrGr,
                                                                            jlong matAddrRgba) {
    // Grayscale and color image from camera.
    Mat &mGr = *(Mat *) matAddrGr;
    Mat &mRgb = *(Mat *) matAddrRgba;

    // Vector of marker ID's in order they were detected.
    vector< int > markerIds;

    // Vector of vectors of 4 points representing marker corners, order is the same as marker ID's.
    // First corner is top left and continuing clockwise.
    vector< vector<Point2f> > markerCorners;

    // ArUco dictionary containing 50 ID's of markers size 4x4.
    Ptr<Dictionary> dictionary = getPredefinedDictionary(DICT_4X4_50);

    try {
        detectMarkers(mGr, dictionary, markerCorners, markerIds);
    } catch (cv::Exception& e) {
        __android_log_print(ANDROID_LOG_VERBOSE, APPNAME, "%s", e.what());
    }

    // Draw only if at least 4 markers were detected, that means octave can be drawn.
    if(markerIds.size() > 3) {
        try {
            draw(mRgb, markerCorners, getSortedIds(markerIds));
        } catch (cv::Exception& e) {
            __android_log_print(ANDROID_LOG_VERBOSE, APPNAME, "%s", e.what());
        }
    }

}

}