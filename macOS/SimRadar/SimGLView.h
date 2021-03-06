//
//  SimGLView.h
//
//  Created by Boon Leng Cheong on 10/29/13.
//  Copyright (c) 2013 Boon Leng Cheong. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "Renderer.h"
//#import "Recorder.h"

@interface SimGLView : NSOpenGLView {
	BOOL animating;
	Renderer *renderer;
//    Recorder *recorder;
@private
	CVDisplayLinkRef displayLink;
    unsigned int tic;
    void *scratchBuffer;
}

@property (readonly, nonatomic, getter = isAnimating) BOOL animating;
@property (readonly) Renderer *renderer;
//@property (nonatomic, retain) Recorder *recorder;

- (void)prepareRendererWithDelegate:(id)sender;
- (void)startAnimation;
- (void)stopAnimation;
- (void)viewToFile:(NSString *)filename;
//- (void)detachRecorder;

@end
