#include "CImg.h"
#include "math.h"
#include "iostream"
#include "string"
#include "vector"
#include "algorithm"
#include "cmath"
#include "dirent.h"
#include "cstring"
#include "chrono"

using namespace cimg_library;
using namespace std;

class Image{
    public:
        string fileName;
        float averageBrightness;
        int width;
        int height;
        vector<vector<int>> smallProfile;
        vector<vector<int>> smallGrayscaleProfile;
};

class Pairing{
    public:
        Image image1;
        Image image2;
        float similarity;
};

vector<string> GetDirectoriesInDirectory(const string& path, bool isFirstLevel);
vector<string> GetImagesInDirectory(const string& path, bool isFirstLevel);
vector<string> GetImageList(string path, bool isRecursive);
long long factorial(int x);
float CompareProfiles(vector<vector<int>> image1, vector<vector<int>> image2);
vector<vector<int>> CreateProfile(CImg<unsigned char> image, int resolution);
vector<vector<int>> CreateProfile2(CImg<unsigned char> image, int resolution);
vector<float> ConvertToYUV(vector<int> colour);
float GetColourSimilarity(vector<int> a, vector<int> b);
float GetYUVColourSimilarity(vector<float> a, vector<float> b);
float GetChannelSimilarity(int a, int b);
void ShowMatches(vector<Pairing> matches);
float GetAspectRatioPenalty(Image image1, Image image2);
string GetTitle(Image img, bool isFirst, double similarity, int id);

const float colourDifferencePenalty = 1.0;//0.78125f;
const bool colourSimilarityUsesAverage = true;//true is less strict = higher percent similar

const bool isGrayscale = true;
float yuvDiffPenalty = 1.3f;
int minimumSimilarity = 90;

bool isSortedBySimilarity = false;
bool isRecursive = true;//searches all sub directories
bool usesAspectRatioPenalty = true;
float aspectRatioPenalty = 0.75f;//multiplied by absolute difference in aspect ratio
int imageLimit = 700;
float resolutionPenalty = 0;
int imageMinimumLength = 4;
string workingDirectory = "./";

int matchesFound = 0;//for GetTitle

int main(int argc, char *argv[]) {
    //TODO: feature: check single image against a directory of images
    //TODO: feature: save profiles for faster future scans (checking modified date/hash to determine if updating needs to be done)

    if(argc >= 2){
        if(strcmp(argv[1], "0") == 0 || strcmp(argv[1], "false") == 0){
            isRecursive = false;
        }
    }
    
    if(argc >= 3){
        workingDirectory = argv[2];
        //TODO if last char isn't / then append one to the end
    }

    cimg_library::cimg::exception_mode(0);
    
    chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();

    cout << "Searching directory \"" << workingDirectory << "\"...\n";
    
    vector<string> files = GetImageList(workingDirectory, isRecursive);
    
    if(files.size() < 2){
        cout << files.size() << " image(s) found.  Minimum is 2.  Exiting.\n";
        return 1;
    }
    
    if(files.size() > imageLimit){
        cout << files.size() << " images found.  Limit is " << imageLimit << ".  Exiting.\n";
        return 2;
    }
    
    cout << files.size() << " images found.  Generating image profiles...\n";
    
    //Create smallProfiles for all images
    vector<Image> images;
    vector<string> ignoredImages;
    for(string fileName : files){
        try{
            CImg<unsigned char> tempCImg(fileName.c_str());
        
            if(tempCImg.width() < imageMinimumLength || tempCImg.height() < imageMinimumLength){
                ignoredImages.push_back(fileName);
            }
            else{
                Image newImage;
                newImage.fileName = fileName;
                newImage.height = tempCImg.height();
                newImage.width = tempCImg.width();
                newImage.smallProfile = CreateProfile(tempCImg, 1);
                images.push_back(newImage);
            }
        }
        catch(CImgIOException e){
            cout << fileName << " has caused an error.  It may not be a valid image file.  Skipping...\n";
        }
        catch(...){
            cout << fileName << " has caused an error.  Skipping...\n";
        }
    }
    
    if(ignoredImages.size() > 0){
        cout << ignoredImages.size() << " image(s) were ignored due to being too small (width or height less than 4).\n";
    }
    
    long long totalChecks = files.size() * (files.size() - 1) / 2;
    cout << "Image profile generation done.  Performing " << totalChecks << " comparisons...\n";
    chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
    auto profileGenerationDuration = chrono::duration_cast<chrono::microseconds>(t2 - t1).count();
    
    //Compare smallProfiles for matches to add to firstPass
    //TODO: look into doing this in parallel
    vector<Pairing> matches;
    for(int i = 0; i < images.size(); i++){
        for(int j = i + 1; j < images.size(); j++){
            float similarity = CompareProfiles(images[i].smallProfile, images[j].smallProfile);
            
            if(usesAspectRatioPenalty){
                float penaltyMultiplier = GetAspectRatioPenalty(images[i], images[j]);
                similarity *= penaltyMultiplier;
            }
            
            if(similarity > minimumSimilarity){
                //cout << images[i].fileName << " and " << images[j].fileName << " are " << similarity << " % similar.\n";
                //TODO: sort by similarity before outputting? -- use an option to determine sorting
                Pairing newPairing;
                newPairing.image1 = images[i];
                newPairing.image2 = images[j];
                newPairing.similarity = similarity;
                matches.push_back(newPairing);
            }
        }
    }

    cout << "Profile generation took " << profileGenerationDuration / (float)1000000 << " seconds.\n";
    //TODO add duration for comparisons
    cout << matches.size() << " matches found.\n";
    
    if(matches.size() > 0){
        string showMatchesResponse;
        cout << "Show matches? (y/n)\n";
        cin >> showMatchesResponse;
        if(showMatchesResponse.compare("y") == 0){
            ShowMatches(matches);
        }
    }
    
    return 0;
}

string GetTitle(Image img, bool isFirst, double similarity, int id){
    string withinPairIdentifier;
    if(isFirst){
        withinPairIdentifier = "a";
    }
    else{
        withinPairIdentifier = "b";
    }
    return "[" + to_string(id+1) + "/" + to_string(matchesFound) + "," + withinPairIdentifier + "]{" + to_string(similarity) + "%}(" + to_string(img.width) + "x" + to_string(img.height) + ") " + img.fileName + "";
}

void ShowMatches(vector<Pairing> matches){
    int currentMatch = 0;
    int originalMatch = 0;//used to detect a change
    Pairing match = matches[currentMatch];
    matchesFound = matches.size();//used to form titles
    cout << "Showing " << matches.size() << " matches.\n";

    CImg<unsigned char> image1CImg(match.image1.fileName.c_str());
    CImg<unsigned char> image2CImg(match.image2.fileName.c_str());
    CImg<unsigned char>* activeImage = &image1CImg;
    CImgDisplay image_display(*activeImage, GetTitle(match.image1, true, match.similarity, currentMatch).c_str());
    
    while(!image_display.is_closed()){
        
        originalMatch = currentMatch;
        
        if(image_display.is_keyENTER() || image_display.is_keyESC()){
            image_display.close();
        }
        
        if(image_display.is_keyD()){//delete current image
            string targetDeletePath = match.image1.fileName;
            
            if(activeImage == &image2CImg){
                targetDeletePath = match.image2.fileName;
                match.image2.fileName = "";
            }
            else{
                match.image1.fileName = "";
            }
            
            if(remove(targetDeletePath.c_str()) != 0){
                cout << "Error deleting " << targetDeletePath << "\n";
            }
            else{
                cout << "Deleted " << targetDeletePath << "\n";
                image_display.set_title(("Deleted " + targetDeletePath).c_str());
                CImg<unsigned char> temp(32,32,1,3,0);
                *activeImage = temp;
                image_display.display(*activeImage);
            }
        }
        
        if(image_display.is_keyHOME()){
            currentMatch = 0;
        }
        
        if(image_display.is_keyEND()){
            currentMatch = matches.size() - 1;
        }
        
        if(image_display.is_keyARROWUP()){
            currentMatch = max(currentMatch - 1, 0);
        }
        
        if(image_display.is_keyARROWDOWN()){
            currentMatch = min(currentMatch + 1, (int)matches.size() - 1);
        }
        
        if(image_display.is_keyF()){
            if(image_display.is_fullscreen()){
                image_display.set_fullscreen(false, true);
            }
            else{
                image_display.set_fullscreen(true, true);
            }
            
        }
    
        if(image_display.is_keyARROWLEFT()){
            activeImage = &image1CImg;
            image_display.display(*activeImage);
            image_display.set_title(GetTitle(match.image1, true, match.similarity, currentMatch).c_str());
        }
        
        if(image_display.is_keyARROWRIGHT()){
            activeImage = &image2CImg;
            image_display.display(*activeImage);
            image_display.set_title(GetTitle(match.image2, false, match.similarity, currentMatch).c_str());
        }
    
        //go to next matching pair
        if(originalMatch != currentMatch){
            match = matches[currentMatch];
 
            try{
                image1CImg.assign(match.image1.fileName.c_str());
            }
            catch(exception& e){
                image1CImg.assign(32,32,1,3,0);
                //title = "Deleted (" + match.image1.fileName + ")";
            }
            
            try{
                image2CImg.assign(match.image2.fileName.c_str());
            }
            catch(exception& e){
                image2CImg.assign(32,32,1,3,0);
                //title = "Deleted (" + match.image2.fileName + ")";
            }
            
            activeImage = &image1CImg;
            image_display.display(*activeImage);
            image_display.set_title(GetTitle(match.image1, true, match.similarity, currentMatch).c_str());
        }
    
        CImgDisplay::wait(image_display);
    }
        
}

vector<float> ConvertToYUV(vector<int> colour){
    float red = colour[0] / 255.0f;
    float green = colour[1] / 255.0f;
    float blue = colour[2] / 255.0f;
    float y = 0.2126f * red + 0.7152f * green + 0.0722f * blue;
    float u = -0.09991f * red - 0.33609f * green + 0.436f * blue;
    float v = 0.615f * red - 0.55861f * green - 0.05639f * blue;
    return vector<float> {y, u, v};
}

float GetAspectRatioPenalty(Image image1, Image image2){
    float image1ar = (float)image1.width / (float)image1.height;
    float image2ar = (float)image2.width / (float)image2.height;
    float multiplier = max(1.0f - aspectRatioPenalty * (abs(image1ar - image2ar)), 0.0f);
    //cout << "ar penalty: " << image1.width << "," << image1.height << " (" << image1ar << ") vs " << image2.width << "," << image2.height << " (" << image2ar << ") yields multiplier of " << multiplier << "\n";
    
    return multiplier;
}

float CompareProfiles(vector<vector<int>> image1, vector<vector<int>> image2){
    float similarSums = 0;
    for(int i = 0; i < image1.size(); i++){
        float nbhSimilarity = GetYUVColourSimilarity(ConvertToYUV(image1[i]), ConvertToYUV(image2[i]));
        similarSums += nbhSimilarity;
    }
    float imageSimilarity = similarSums / image1.size();
    return imageSimilarity; 
}

float GetYUVColourSimilarity(vector<float> a, vector<float> b){
    float yDiff = abs(a[0] - b[0]);
    float diff = yDiff;
    if(!isGrayscale){
        //the smaller the y value, the less important u and v are, thus they are scaled based off of y
        float uDiff = abs(a[1] - b[1]) * yDiff * 2.0f;
        float vDiff = abs(a[2] - b[2]) * yDiff * 2.0f;
        diff = (yDiff + uDiff + vDiff);
    }
    float similarity = 100.0f - (max(0.0f, min(1.0f, diff * yuvDiffPenalty))) * 100.0f;
    return similarity;
}

vector<vector<int>> CreateProfile(CImg<unsigned char> image, int resolution){
    vector<vector<int>> profile;
    //TODO hook width and height to resolution? create multiple profiles then
    int width = 16, height = 16;
    
    CImg<unsigned char> thumb = image.resize(width, height);
    
    /*
    CImg<unsigned char> activeImage = thumb;
    CImgDisplay image_display(activeImage, "");
    while(!image_display.is_closed()){
        CImgDisplay::wait(image_display);
    }
    */
    
    for(int x = 0; x < width; x++){
        for(int y = 0; y < height; y++){

            vector<int> temp = {thumb(x, y, 0, 0), thumb(x, y, 0, 1), thumb(x, y, 0, 2)};
            profile.push_back(temp);
        }
    }

    return profile;
}

long long factorial(int x){
    if(x <= 0){
        return 0;
    }

    long long result = x;
    
    while(x > 1){
        x--;
        result *= x;
    }

    return result;
}

vector<string> GetImageList(string path, bool isRecursive){
    vector<string> files;
    vector<string> directories;
    
    directories.push_back(path);
    int maxLoops = 1000;
    int currentLoop = 0;
    while(directories.size() > 0){
        path = directories[directories.size() - 1];
        directories.pop_back();
        
        vector<string> foundImages = GetImagesInDirectory(path, (currentLoop == 0 ? true : false));
        files.insert(files.end(), foundImages.begin(), foundImages.end());
        
        if(isRecursive){
            vector<string> foundDirectories = GetDirectoriesInDirectory(path, (currentLoop == 0 ? true : false));
            directories.insert(directories.end(), foundDirectories.begin(), foundDirectories.end());
        }
        
        currentLoop++;
        if(currentLoop > maxLoops){
            break;
        }
        
    }
    
    return files;
}

vector<string> GetImagesInDirectory(const string& path, bool isFirstLevel){
    vector<string> imageList;
    DIR *dirPointer;
    struct dirent *entry;
    dirPointer = opendir(path.c_str());
    if(dirPointer != NULL){
        while ((entry = readdir(dirPointer)) != NULL){
            string entryName = entry->d_name;
            if(entry->d_type != DT_DIR && ((entryName.find(".jpg") != string::npos) || entryName.find(".png") != string::npos)){
                //TODO: verify it's at the end of the string before adding to list
                //TODO possibly check first few bytes to verify it's an image? time cost related to doing this?
                if(isFirstLevel){
                    imageList.push_back(path + entry->d_name);
                }
                else{
                    imageList.push_back(path + "/" + entry->d_name);
                }
            }        
        }
    }
    closedir(dirPointer);
    return imageList;
}

vector<string> GetDirectoriesInDirectory(const string& path, bool isFirstLevel){
    vector<string> directoryList;
    DIR *dirPointer;
    struct dirent *entry;
    dirPointer = opendir(path.c_str());
    if(dirPointer != NULL){
        while ((entry = readdir(dirPointer)) != NULL){
            if(entry->d_type == DT_DIR){
                if(string(entry->d_name).compare(".") != 0 && string(entry->d_name).compare("..") != 0){
                    string fullDirectory;
                    if(isFirstLevel){
                        fullDirectory = path + entry->d_name;
                        directoryList.push_back(path + entry->d_name);
                    }
                    else{
                        fullDirectory = path + "/" + entry->d_name;
                        directoryList.push_back(path + "/" + entry->d_name);
                    }
                    //cout << "Found directory: " << fullDirectory << "\n";
                }
            }       
        }
    }
    closedir(dirPointer);
    return directoryList;
}

