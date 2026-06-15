#include "scanlation_tool/ocr.hpp"

#import <Foundation/Foundation.h>
#import <CoreImage/CoreImage.h>
#import <Vision/Vision.h>

#include <memory>
#include <string>

namespace scanlation {

namespace {

std::string join_lines(NSArray<VNRecognizedTextObservation*>* observations) {
  NSMutableArray<NSString*>* lines = [NSMutableArray array];
  for (VNRecognizedTextObservation* observation in observations) {
    NSArray<VNRecognizedText*>* candidates = [observation topCandidates:1];
    if ([candidates count] > 0) {
      NSString* text = [[candidates objectAtIndex:0] string];
      if (text != nil && [text length] > 0) {
        [lines addObject:[text lowercaseString]];
      }
    }
  }
  NSString* joined = [lines componentsJoinedByString:@" "];
  return joined == nil ? std::string() : std::string([joined UTF8String]);
}

class AppleVisionBackend final : public IOcrBackend {
 public:
  std::string name() const override { return "Apple Vision"; }

  bool available() const override {
    if (@available(macOS 10.15, *)) {
      return true;
    }
    return false;
  }

  std::string read_text(const std::filesystem::path& image_path) const override {
    @autoreleasepool {
      if (!available()) {
        return "";
      }
      NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:image_path.string().c_str()]];
      CIImage* image = [CIImage imageWithContentsOfURL:url];
      if (image == nil) {
        return "";
      }

      __block std::string output;
      VNRecognizeTextRequest* request = [[VNRecognizeTextRequest alloc]
          initWithCompletionHandler:^(VNRequest* req, NSError* error) {
            if (error != nil) {
              return;
            }
            output = join_lines((NSArray<VNRecognizedTextObservation*>*)[req results]);
          }];
      [request setRecognitionLevel:VNRequestTextRecognitionLevelAccurate];
      [request setUsesLanguageCorrection:NO];

      VNImageRequestHandler* handler = [[VNImageRequestHandler alloc] initWithCIImage:image options:@{}];
      NSError* error = nil;
      BOOL ok = [handler performRequests:@[ request ] error:&error];
      if (!ok || error != nil) {
        return "";
      }
      return output;
    }
  }
};

}  // namespace

std::unique_ptr<IOcrBackend> make_apple_vision_backend() {
  return std::make_unique<AppleVisionBackend>();
}

}  // namespace scanlation
