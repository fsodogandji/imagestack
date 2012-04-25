#include "main.h"
#include "Filter.h"
#include "Convolve.h"
#include "Color.h"
#include "Geometry.h"
#include "Arithmetic.h"
#include "header.h"

void GaussianBlur::help() {
    pprintf("-gaussianblur takes a floating point width, height, and frames, and"
            " performs a gaussian blur with those standard deviations. The blur is"
            " performed out to three standard deviations. If given only two"
            " arguments, it performs a blur in x and y only. If given one argument,"
            " it performs the blur in x and y with filter width the same as"
            " height.\n"
            "\n"
            "Usage: ImageStack -load in.jpg -gaussianblur 5 -save blurry.jpg\n\n");
}

void GaussianBlur::parse(vector<string> args) {
    float frames = 0, width = 0, height = 0;
    if (args.size() == 1) {
        width = height = readFloat(args[0]);
    } else if (args.size() == 2) {
        width = readFloat(args[0]);
        height = readFloat(args[1]);
    } else if (args.size() == 3) {
        width  = readFloat(args[0]);
        height = readFloat(args[1]);
        frames = readFloat(args[2]);
    } else {
        panic("-gaussianblur takes one, two, or three arguments\n");
    }

    NewImage im = apply(stack(0), width, height, frames);
    pop();
    push(im);
}

NewImage GaussianBlur::apply(NewImage im, float filterWidth, float filterHeight, float filterFrames) {
    NewImage out(im);

    if (filterWidth != 0) {
        // make the width filter
        int size = (int)(filterWidth * 6 + 1) | 1;
        // even tiny filters should do something, otherwise we
        // wouldn't have called this function.
        if (size == 1) { size = 3; }
        int radius = size / 2;
        NewImage filter(size, 1, 1, 1);
        float sum = 0;
        for (int i = 0; i < size; i++) {
            float diff = (i-radius)/filterWidth;
            float value = expf(-diff * diff / 2);
            filter(i, 0, 0, 0) = value;
            sum += value;
        }

        for (int i = 0; i < size; i++) {
            filter(i, 0, 0, 0) /= sum;
        }

        out = Convolve::apply(out, filter);
    }

    if (filterHeight != 0) {
        // make the height filter
        int size = (int)(filterHeight * 6 + 1) | 1;
        // even tiny filters should do something, otherwise we
        // wouldn't have called this function.
        if (size == 1) { size = 3; }
        int radius = size / 2;
        NewImage filter(1, size, 1, 1);
        float sum = 0;
        for (int i = 0; i < size; i++) {
            float diff = (i-radius)/filterHeight;
            float value = expf(-diff * diff / 2);
            filter(0, i, 0, 0) = value;
            sum += value;
        }

        for (int i = 0; i < size; i++) {
            filter(0, i, 0, 0) /= sum;
        }

        out = Convolve::apply(out, filter);
    }

    if (filterFrames != 0) {
        // make the frames filter
        int size = (int)(filterFrames * 6 + 1) | 1;
        // even tiny filters should do something, otherwise we
        // wouldn't have called this function.
        if (size == 1) { size = 3; }
        int radius = size / 2;
        NewImage filter(1, 1, size, 1);
        float sum = 0;
        for (int i = 0; i < size; i++) {
            float diff = (i-radius)/filterFrames;
            float value = expf(-diff * diff / 2);
            filter(0, 0, i, 0) = value;
            sum += value;
        }

        for (int i = 0; i < size; i++) {
            filter(0, 0, i, 0) /= sum;
        }

        out = Convolve::apply(out, filter);
    }

    return out;
}

// This blur implementation was contributed by Tyler Mullen as a
// CS448F project. A competition was held, and this method was found
// to be much faster than other IIRs, filtering by resampling,
// iterated rect filters, and polynomial integral images. The method
// was modified by Andrew Adams to be more ImageStacky (i.e. use
// structures more idiomatic to ImageStack like pointer marching), to
// work for larger sized blurs, and to cover more unusual cases.

void FastBlur::help() {
    pprintf("-fastblur takes a floating point width, height, and frames, and"
            " performs a fast approximate gaussian blur with those standard"
            " deviations using the IIR method of van Vliet et al. If given only two"
            " arguments, it performs a blur in x and y only. If given one argument,"
            " it performs the blur in x and y with filter width the same as"
            " height.\n"
            "\n"
            "Usage: ImageStack -load in.jpg -fastblur 5 -save blurry.jpg\n\n");
}

void FastBlur::parse(vector<string> args) {
    float frames = 0, width = 0, height = 0;
    if (args.size() == 1) {
        width = height = readFloat(args[0]);
    } else if (args.size() == 2) {
        width = readFloat(args[0]);
        height = readFloat(args[1]);
    } else if (args.size() == 3) {
        width  = readFloat(args[0]);
        height = readFloat(args[1]);
        frames = readFloat(args[2]);
    } else {
        panic("-fastblur takes one, two, or three arguments\n");
    }

    apply(stack(0), width, height, frames);
}

void FastBlur::apply(NewImage im, float filterWidth, float filterHeight, float filterFrames, bool addMargin) {
    assert(filterFrames >= 0 &&
           filterWidth >= 0 &&
           filterHeight >= 0,
           "Filter sizes must be non-negative\n");

    // Prevent filtering in useless directions
    if (im.width == 1) { filterWidth = 0; }
    if (im.height == 1) { filterHeight = 0; }
    if (im.frames == 1) { filterFrames = 0; }

    //printf("%d %d %d\n", im.width, im.height, im.frames);

    // Filter in very narrow directions using the regular Gaussian, as
    // the IIR requires a few pixels to get going. If the Gaussian
    // blur is very narrow, also revert to the naive method, as IIR
    // won't work.
    if (filterFrames > 0 && (im.frames < 16 || filterFrames < 0.5)) {
        NewImage blurry = GaussianBlur::apply(im, filterFrames, 0, 0);
        FastBlur::apply(blurry, filterWidth, filterHeight, 0);
        Paste::apply(im, blurry, 0, 0, 0);
        return;
    }

    if (filterWidth > 0 && (im.width < 16 || filterWidth < 0.5)) {
        NewImage blurry = GaussianBlur::apply(im, 0, filterWidth, 0);
        FastBlur::apply(blurry, 0, filterHeight, filterFrames);
        Paste::apply(im, blurry, 0, 0, 0);
        return;
    }

    if (filterHeight > 0 && (im.height < 16 || filterHeight < 0.5)) {
        NewImage blurry = GaussianBlur::apply(im, 0, 0, filterHeight);
        FastBlur::apply(blurry, filterWidth, 0, filterFrames);
        Paste::apply(im, blurry, 0, 0, 0);
        return;
    }

    // IIR filtering fails if the std dev is similar to the image
    // size, because it displays a bias towards the edge values on the
    // starting side. We solve this by adding a margin and using
    // homogeneous weights.
    if (addMargin && (im.frames / filterFrames < 8 ||
                      im.width / filterWidth < 8 ||
                      im.height / filterHeight < 8)) {

        int marginT = (int)(filterFrames);
        int marginX = (int)(filterWidth);
        int marginY = (int)(filterHeight);

        NewImage bigger(im.width+2*marginX, im.height+2*marginY, im.frames+2*marginT, im.channels+1);
        for (int t = 0; t < im.frames; t++) {
            for (int y = 0; y < im.height; y++) {
                for (int x = 0; x < im.width; x++) {
                    bigger(x+marginX, y+marginY, t+marginT, im.channels) = 1;
                    for (int c = 0; c < im.channels; c++) {
                        bigger(x+marginX, y+marginY, t+marginT, c) = im(x, y, t, c);
                    }
                }
            }
        }

        FastBlur::apply(bigger, filterFrames, filterWidth, filterHeight, false);
        for (int t = 0; t < im.frames; t++) {
            for (int y = 0; y < im.height; y++) {
                for (int x = 0; x < im.width; x++) {
                    float w = 1.0f/bigger(x+marginX, y+marginY, t+marginT, im.channels);
                    for (int c = 0; c < im.channels; c++) {
                        im(x, y, t, c) = w*bigger(x+marginX, y+marginY, t+marginT, c);
                    }
                }
            }
        }

        return;
    }

    // now perform the blur
    if (filterWidth > 32) {
        // for large filters, we decompose into a dense blur and a
        // sparse blur, by spacing out the taps on the IIR
        float remainingStdDev = sqrtf(filterWidth*filterWidth - 32*32);
        int tapSpacing = (int)(remainingStdDev / 32 + 1);
        blurX(im, remainingStdDev/tapSpacing, tapSpacing);
        blurX(im, 32, 1);
    } else if (filterWidth > 0) {
        blurX(im, filterWidth, 1);
    }

    if (filterHeight > 32) {
        float remainingStdDev = sqrtf(filterHeight*filterHeight - 32*32);
        int tapSpacing = (int)(remainingStdDev / 32 + 1);
        blurY(im, remainingStdDev/tapSpacing, tapSpacing);
        blurY(im, 32, 1);
    } else if (filterHeight > 0) {
        blurY(im, filterHeight, 1);
    }

    if (filterFrames > 32) {
        float remainingStdDev = sqrtf(filterFrames*filterFrames - 32*32);
        int tapSpacing = (int)(remainingStdDev / 32 + 1);
        blurT(im, remainingStdDev/tapSpacing, tapSpacing);
        blurT(im, 32, 1);
    } else if (filterFrames > 0) {
        blurT(im, filterFrames, 1);
    }
}

void FastBlur::blurX(NewImage im, float sigma, int ts) {
    if (sigma == 0) { return; }

    // blur in the x-direction
    float c0, c1, c2, c3;
    calculateCoefficients(sigma, &c0, &c1, &c2, &c3);

    float invC01 = 1.0f/(c0+c1);
    float invC012 = 1.0f/(c0+c1+c2);

    // we step through each row of each frame, and apply a forwards and then
    // a backwards pass of our IIR filter to approximate Gaussian blurring
    // in the x-direction
    for (int c = 0; c < im.channels; c++) {
        for (int t = 0; t < im.frames; t++) {
            for (int y = 0; y < im.height; y++) {
                // forward pass
                
                // use a zero boundary condition in the homogeneous
                // sense (ie zero weight outside the image, divide by
                // the sum of the weights)
                for (int j = 0; j < ts; j++) {
                    im(ts+j, y, t, c) = (c0*im(ts+j, y, t, c) + c1*im(j, y, t, c)) * invC01;
                    im(2*ts+j, y, t, c) = (c0*im(2*ts+j, y, t, c) + c1*im(ts+j, y, t, c) + c2*im(j, y, t, c)) * invC012;
                }
                
                // now apply the forward filter
                for (int x = 3*ts; x < im.width; x++) {
                    im(x, y, t, c) = (c0 * im(x, y, t, c) +
                                      c1 * im(x-ts, y, t, c) +
                                      c2 * im(x-2*ts, y, t, c) + 
                                      c3 * im(x-3*ts, y, t, c));
                }

                // use a zero boundary condition in the homogeneous
                // sense
                int x = im.width-3*ts;
                for (int j = 0; j < ts; j++) {
                    im(x+ts+j, y, t, c) = (c0*im(x+ts+j, y, t, c) + c1*im(x+2*ts+j, y, t, c)) * invC01;
                    im(x+j, y, t, c) = (c0*im(x+j, y, t, c) + c1*im(x+ts+j, y, t, c) + c2*im(x+2*ts+j, y, t, c)) * invC012;
                }
                
                // backward pass
                for (int x = im.width-3*ts-1; x >= 0; x--) {
                    im(x, y, t, c) = (c0 * im(x, y, t, c) + 
                                      c1 * im(x+ts, y, t, c) + 
                                      c2 * im(x+2*ts, y, t, c) + 
                                      c3 * im(x+3*ts, y, t, c));
                }
            }
        }
    }
}

void FastBlur::blurY(NewImage im, float sigma, int ts) {
    if (sigma == 0) { return; }

    float c0, c1, c2, c3;
    calculateCoefficients(sigma, &c0, &c1, &c2, &c3);
    float invC01 = 1.0f/(c0+c1);
    float invC012 = 1.0f/(c0+c1+c2);

    // blur in the y-direction
    //  we do the same thing here as in the x-direction
    //  but we apply im.width different filters in parallel,
    //  for cache coherency's sake, first all going in the "forwards"
    //  direction, and then all going in the "backwards" direction
    for (int c = 0; c < im.channels; c++) {
        for (int t = 0; t < im.frames; t++) {
            // use a zero boundary condition in the homogeneous
            // sense (ie zero weight outside the image, divide by
            // the sum of the weights)
            for (int j = 0; j < ts; j++) {
                for (int x = 0; x < im.width; x++) {
                    im(x, ts+j, t, c) = (c0*im(x, ts+j, t, c) + c1*im(x, j, t, c)) * invC01;
                    im(x, 2*ts+j, t, c) = (c0*im(x, 2*ts+j, t, c) + c1*im(x, ts+j, t, c) + c2*im(x, j, t, c)) * invC012;
                }
            }

            // forward pass
            for (int y = 3*ts; y < im.height; y++) {
                for (int x = 0; x < im.width; x++) {
                    im(x, y, t, c) = (c0 * im(x, y, t, c) + 
                                      c1 * im(x, y-ts, t, c) + 
                                      c2 * im(x, y-2*ts, t, c) + 
                                      c3 * im(x, y-3*ts, t, c));
                }
            }
                
            // use a zero boundary condition in the homogeneous
            // sense (ie zero weight outside the image, divide by
            // the sum of the weights)
            int y = im.height-3*ts;
            for (int j = 0; j < ts; j++) {
                for (int x = 0; x < im.width; x++) {
                    im(x, y+ts+j, t, c) = (c0*im(x, y+ts+j, t, c) + c1*im(x, y+ts*2+j, t, c)) * invC01;
                    im(x, y+j, t, c) = (c0*im(x, y+j, t, c) + c1*im(x, y+ts+j, t, c) + c2*im(x, y+ts*2+j, t, c)) * invC012;
                }
            }
            
            // backward pass
            for (int y = im.height-3*ts-1; y >= 0; y--) {
                for (int x = 0; x < im.width; x++) 
                    im(x, y, t, c) = (c0 * im(x, y, t, c) + 
                                      c1 * im(x, y+ts, t, c) + 
                                      c2 * im(x, y+2*ts, t, c) + 
                                      c3 * im(x, y+3*ts, t, c));
            }
        }
    }
}

void FastBlur::blurT(NewImage im, float sigma, int ts) {
    if (sigma == 0) { return; }

    float c0, c1, c2, c3;
    calculateCoefficients(sigma, &c0, &c1, &c2, &c3);
    float invC01 = 1.0f/(c0+c1);
    float invC012 = 1.0f/(c0+c1+c2);

    // blur in the t-direction
    // this is the same strategy as blurring in y, but we swap t
    // for y everywhere
    for (int c = 0; c < im.channels; c++) {
        for (int y = 0; y < im.height; y++) {
            // use a zero boundary condition in the homogeneous
            // sense (ie zero weight outside the image, divide by
            // the sum of the weights)
            for (int j = 0; j < ts; j++) {
                for (int x = 0; x < im.width; x++) {
                    im(x, y, ts+j, c) = (c0*im(x, y, ts+j, c) + c1*im(x, y, j, c)) * invC01;
                    im(x, y, 2*ts+j, c) = (c0*im(x, y, 2*ts+j, c) + c1*im(x, y, ts+j, c) + c2*im(x, y, j, c)) * invC012;
                }
            }
            
            // forward pass
            
            for (int t = 3*ts; t < im.frames; t++) {
                for (int x = 0; x < im.width; x++) {
                    im(x, y, t, c) = (c0 * im(x, y, t, c) + 
                                      c1 * im(x, y, t-ts, c) + 
                                      c2 * im(x, y, t-2*ts, c) + 
                                      c3 * im(x, y, t-3*ts, c));
                }
            }
            
            // use a zero boundary condition in the homogeneous
            // sense (ie zero weight outside the image, divide by
            // the sum of the weights)
            int t = im.frames-3*ts;
            for (int j = 0; j < ts; j++) {
                for (int x = 0; x < im.width; x++) {
                    im(x, y, t+ts+j, c) = (c0*im(x, y, t+ts+j, c) + c1*im(x, y, t+2*ts+j, c)) * invC01;
                    im(x, y, t+j, c) = (c0*im(x, y, t+j, c) + c1*im(x, y, t+ts+j, c) + c2*im(x, y, t+2*ts+j, c)) * invC012;
                }
            }
            
            // backward pass
            for (int t = im.frames-3*ts-1; t >= 0; t--) {
                for (int x = 0; x < im.width; x++) {
                    im(x, y, t, c) = (c0 * im(x, y, t, c) + 
                                      c1 * im(x, y, t+ts, c) + 
                                      c2 * im(x, y, t+2*ts, c) + 
                                      c3 * im(x, y, t+3*ts, c));
                }
            }
        }
    }
}

void FastBlur::calculateCoefficients(float sigma, float *c0, float *c1, float *c2, float *c3) {
    // performs the necessary conversion between the sigma of a Gaussian blur
    // and the coefficients used in the IIR filter

    float q;

    assert(sigma >= 0.5, "To use IIR filtering, standard deviation of blur must be >= 0.5\n");

    if (sigma < 2.5) {
        q = (3.97156 - 4.14554*sqrtf(1 - 0.26891*sigma));
    } else {
        q = 0.98711*sigma - 0.96330;
    }

    float denom = 1.57825 + 2.44413*q + 1.4281*q*q + 0.422205*q*q*q;
    *c1 = (2.44413*q + 2.85619*q*q + 1.26661*q*q*q)/denom;
    *c2 = -(1.4281*q*q + 1.26661*q*q*q)/denom;
    *c3 = (0.422205*q*q*q)/denom;
    *c0 = 1 - (*c1 + *c2 + *c3);
}

void RectFilter::help() {
    pprintf("-rectfilter performs a iterated rectangular filter on the image. The"
            " four arguments are the filter width, height, frames, and the number of"
            " iterations. If three arguments are given, they are interpreted as"
            " frames, width, and height, and the number of iterations is assumed to"
            " be one. If two arguments are given they are taken as width and height,"
            " and frames is assumed to be one. If one argument is given it is taken"
            " as both width and height, with frames and iterations again assumed to"
            " be one.\n"
            "\n"
            "Usage: ImageStack -load in.jpg -rectfilter 1 10 10 -save out.jpg\n\n");
}

void RectFilter::parse(vector<string> args) {
    int iterations = 1, frames = 1, width = 1, height = 1;
    if (args.size() == 1) {
        width = height = readInt(args[0]);
    } else if (args.size() == 2) {
        width = readInt(args[0]);
        height = readInt(args[1]);
    } else if (args.size() == 3) {
        width = readInt(args[0]);
        height = readInt(args[1]);
        frames = readInt(args[2]);
    } else if (args.size() == 4) {
        width = readInt(args[0]);
        height = readInt(args[1]);
        frames = readInt(args[2]);
        iterations = readInt(args[3]);
    } else {
        panic("-rectfilter takes four or fewer arguments\n");
    }

    apply(stack(0), width, height, frames, iterations);
}

void RectFilter::apply(NewImage im, int filterWidth, int filterHeight, int filterFrames, int iterations) {
    assert(filterFrames & filterWidth & filterHeight & 1, "filter shape must be odd\n");
    assert(iterations >= 1, "iterations must be at least one\n");

    if (filterFrames != 1) { blurT(im, filterFrames, iterations); }
    if (filterWidth  != 1) { blurX(im, filterWidth, iterations); }
    if (filterHeight != 1) { blurY(im, filterHeight, iterations); }
}

void RectFilter::blurXCompletely(NewImage im) {
    for (int c = 0; c < im.channels; c++) {
        for (int t = 0; t < im.frames; t++) {
            for (int y = 0; y < im.height; y++) {
                // compute the average for this scanline
                double average = 0;
                for (int x = 0; x < im.width; x++) {
                    average += im(x, y, t, c);
                }
                average /= im.width;
                for (int x = 0; x < im.width; x++) {
                    im(x, y, t, c) = average;
                }
            }
        }
    }
}


void RectFilter::blurX(NewImage im, int width, int iterations) {
    if (width <= 1) { return; }
    if (im.width == 1) { return; }

    // special case where the radius is large enough that the image is totally uniformly blurred
    if (im.width <= width/2) {
        blurXCompletely(im);
        return;
    }

    int radius = width/2;
    vector<float> buffer(width);

    vector<float> multiplier(width);
    for (int i = 0; i < width; i++) {
        multiplier[i] = 1.0f/width;
    }

    for (int c = 0; c < im.channels; c++) {
        for (int t = 0; t < im.frames; t++) {
            for (int y = 0; y < im.height; y++) {
                for (int i = 0; i < iterations; i++) {
                    // keep a circular buffer of everything currently inside the kernel
                    // also maintain the sum of this buffer

                    double sum = 0;
                    int bufferIndex = 0;
                    int bufferEntries = 0;

                    // initialize the buffer
                    for (int j = 0; j <= radius; j++) {
                        buffer[j] = 0;
                    }
                    for (int j = radius+1; j < width; j++) {
                        buffer[j] = im(j-radius, y, t, c);
                        sum += buffer[j];
                        bufferEntries++;
                    }

                    double mult = 1.0/bufferEntries;

                    // non boundary cases
                    for (int x = 0; x < im.width-radius-1; x++) {
                        // assign the average to the current position
                        im(x, y, t, c) = (float)(sum * mult);

                        // swap out the buffer element, updating the sum
                        float newVal = im(x+radius+1, y, t, c);
                        sum += newVal - buffer[bufferIndex];
                        buffer[bufferIndex] = newVal;
                        bufferIndex++;
                        if (bufferIndex == width) { bufferIndex = 0; }

                        if (bufferEntries < width) {
                            bufferEntries++;
                            mult = 1.0/bufferEntries;
                        }
                    }

                    // boundary cases
                    for (int x = im.width-radius-1; x < im.width; x++) {
                        // assign the average to the current position
                        im(x, y, t, c) = (float)(sum * mult);

                        // swap out the buffer element, updating the sum
                        sum -= buffer[bufferIndex];
                        //buffer[bufferIndex] = 0;
                        bufferIndex++;
                        if (bufferIndex == width) { bufferIndex = 0; }

                        bufferEntries--;
                        mult = 1.0/bufferEntries;
                    }
                }
            }
        }
    }

}

void RectFilter::blurY(NewImage im, int width, int iterations) {
    if (width <= 1) { return; }
    if (im.height == 1) { return; }

    // pull out strips of columns and blur them
    NewImage chunk(im.height, 8, 1, 1);

    for (int c = 0; c < im.channels; c++) {
        for (int t = 0; t < im.frames; t++) {
            for (int x = 0; x < im.width; x += chunk.height) {
                int size = chunk.height;
                if (x + chunk.height >= im.width) { size = im.width-x; }

                // read into the chunk in a transposed fashion
                for (int y = 0; y < im.height; y++) {
                    for (int j = 0; j < size; j++) {
                        chunk(y, j) = im(x+j, y, t, c);
                    }
                }

                // blur the chunk
                blurX(chunk, width, iterations);

                // read back from the chunk
                for (int y = 0; y < im.height; y++) {
                    for (int j = 0; j < size; j++) {
                        im(x+j, y, t, c) = chunk(y, j);
                    }
                }
            }
        }
    }
}

void RectFilter::blurT(NewImage im, int width, int iterations) {
    if (width <= 1) { return; }
    if (im.frames == 1) { return; }

    // pull out strips across frames from rows and blur them
    NewImage chunk(im.frames, 8, 1, 1);

    for (int c = 0; c < im.channels; c++) {
        for (int y = 0; y < im.height; y++) {
            for (int x = 0; x < im.width; x += chunk.height) {
                int size = chunk.height;
                if (x + chunk.height >= im.width) { size = im.width-x; }

                // read into the chunk in a transposed fashion
                for (int t = 0; y < im.frames; t++) {
                    for (int j = 0; j < size; j++) {
                        chunk(t, j) = im(x+j, y, t, c);
                    }
                }

                // blur the chunk
                blurX(chunk, width, iterations);

                // read back from the chunk
                for (int t = 0; t < im.frames; t++) {
                    for (int j = 0; j < size; j++) {
                        im(x+j, y, t, c) = chunk(t, j);
                    }
                }
            }
        }
    }
}

void LanczosBlur::help() {
    pprintf("-lanczosblur convolves the current image by a three lobed lanczos"
            " filter. A lanczos filter is a kind of windowed sinc. The three"
            " arguments are filter width, height, and frames. If two arguments are"
            " given, frames is assumed to be one. If one argument is given, it is"
            " interpreted as both width and height.\n"
            "\n"
            "Usage: ImageStack -load big.jpg -lanczosblur 2 -subsample 2 2 0 0 -save small.jpg\n\n");

}

void LanczosBlur::parse(vector<string> args) {
    float frames = 0, width = 0, height = 0;
    if (args.size() == 1) {
        width = height = readFloat(args[0]);
    } else if (args.size() == 2) {
        width = readFloat(args[0]);
        height = readFloat(args[1]);
    } else if (args.size() == 3) {
        width  = readFloat(args[0]);
        height = readFloat(args[1]);
        frames = readFloat(args[2]);
    } else {
        panic("-lanczosblur takes one, two, or three arguments\n");
    }

    NewImage im = apply(stack(0), width, height, frames);
    pop();
    push(im);
}

NewImage LanczosBlur::apply(NewImage im, float filterWidth, float filterHeight, float filterFrames) {
    NewImage out(im);

    if (filterFrames != 0) {
        // make the frames filter
        int size = (int)(filterFrames * 6 + 1) | 1;
        int radius = size / 2;
        NewImage filter(1, 1, size, 1);
        float sum = 0;
        for (int i = 0; i < size; i++) {
            float value = lanczos_3((i-radius) / filterFrames);
            filter(0, 0, i, 0) = value;
            sum += value;
        }

        for (int i = 0; i < size; i++) {
            filter(0, 0, i, 0) /= sum;
        }

        out = Convolve::apply(out, filter);
    }

    if (filterWidth != 0) {
        // make the width filter
        int size = (int)(filterWidth * 6 + 1) | 1;
        int radius = size / 2;
        NewImage filter(size, 1, 1, 1);
        float sum = 0;
        for (int i = 0; i < size; i++) {
            float value = lanczos_3((i-radius) / filterWidth);
            filter(i, 0, 0, 0) = value;
            sum += value;
        }

        for (int i = 0; i < size; i++) {
            filter(i, 0, 0, 0) /= sum;
        }

        out = Convolve::apply(out, filter);
    }

    if (filterHeight != 0) {
        // make the height filter
        int size = (int)(filterHeight * 6 + 1) | 1;
        int radius = size / 2;
        NewImage filter(1, size, 1, 1);
        float sum = 0;
        for (int i = 0; i < size; i++) {
            float value = lanczos_3((i-radius) / filterHeight);
            filter(0, i, 0, 0) = value;
            sum += value;
        }

        for (int i = 0; i < size; i++) {
            filter(0, i, 0, 0) /= sum;
        }

        out = Convolve::apply(out, filter);
    }

    return out;

}


void MinFilter::help() {
    pprintf("-minfilter applies a min filter with square support. The sole argument "
            "is the pixel radius of the filter. For circular support, see "
            "-percentilefilter.\n"
            "\n"
            "Usage: ImageStack -load input.jpg -minfilter 10 -save output.jpg\n");
}

void MinFilter::parse(vector<string> args) {
    assert(args.size() == 1, "-minfilter takes on argument\n");
    int radius = readInt(args[0]);
    assert(radius > -1, "radius must be positive");
    apply(stack(0), radius);
}

void MinFilter::apply(NewImage im, int radius) {
    // Make a heap with (2*radius + 1) leaves. Unlike a regular heap,
    // each internal node is a _copy_ of the smaller child. The leaf
    // nodes act as a circular buffer. Every time we introduce a new
    // pixel (and evict an old one), we update all of its parents up
    // to the root.

    vector<float> heap(4*radius+1);
    
    for (int t = 0; t < im.frames; t++) {
        for (int y = 0; y < im.height; y++) {
            for (int c = 0; c < im.channels; c++) {
                // Initialize the heap to contain all inf
                std::fill(heap.begin(), heap.end(), INF);
                size_t pos = 2*radius;
                for (int x = 0; x < im.width + radius; x++) {                                        
                    // Get the next input
                    float val;
                    if (x < im.width) val = im(x, y, t, c);
                    else val = INF;
                    // Stuff it in the heap                    
                    heap[pos] = val;
                    // Update parents
                    size_t p = pos;
                    do {
                        p--; p>>=1;
                        heap[p] = min(heap[2*p+1], heap[2*p+2]);
                    } while (p);

                    // Maybe write out min
                    if (x-radius > 0)
                        im(x-radius, y, t, c) = heap[0];
                    // Update position in circular buffer
                    pos++;
                    if (pos == heap.size()) pos = 2*radius;
                }
            }
        }

        for (int x = 0; x < im.width; x++) {
            for (int c = 0; c < im.channels; c++) {
                // Initialize the heap to contain all inf
                std::fill(heap.begin(), heap.end(), INF);
                size_t pos = 2*radius;
                for (int y = 0; y < im.height + radius; y++) {                                        
                    float val;
                    if (y < im.height) val = im(x, y, t, c);
                    else val = INF;
                    // stuff it in the heap                    
                    heap[pos] = val;
                    // update parents
                    size_t p = pos;
                    do {
                        p--; p>>=1;
                        heap[p] = min(heap[2*p+1], heap[2*p+2]);
                    } while (p);
                    // write out min
                    if (y-radius > 0) 
                        im(x, y-radius, t, c) = heap[0];
                    // update position in circular buffer
                    pos++;
                    if (pos == heap.size()) pos = 2*radius;
                }
            }
        }
    }

}

void MaxFilter::help() {
    pprintf("-maxfilter applies a max filter with square support. The sole argument "
            "is the pixel radius of the filter. For circular support, see "
            "-percentilefilter.\n"
            "\n"
            "Usage: ImageStack -load input.jpg -maxfilter 10 -save output.jpg\n");
}

void MaxFilter::parse(vector<string> args) {
    assert(args.size() == 1, "-maxfilter takes on argument\n");
    int radius = readInt(args[0]);
    assert(radius > -1, "radius must be positive");
    apply(stack(0), radius);
}

void MaxFilter::apply(NewImage im, int radius) {
    // Make a heap with (2*radius + 1) leaves. Unlike a regular heap,
    // each internal node is a _copy_ of the smaller child. The leaf
    // nodes act as a circular buffer. Every time we introduce a new
    // pixel (and evict an old one), we update all of its parents up
    // to the root.

    vector<float> heap(4*radius+1);
    
    for (int t = 0; t < im.frames; t++) {
        for (int y = 0; y < im.height; y++) {
            for (int c = 0; c < im.channels; c++) {
                // Initialize the heap to contain all inf
                std::fill(heap.begin(), heap.end(), -INF);
                size_t pos = 2*radius;
                for (int x = 0; x < im.width + radius; x++) {                                        
                    // Get the next input
                    float val;
                    if (x < im.width) val = im(x, y, t, c);
                    else val = -INF;
                    // Stuff it in the heap                    
                    heap[pos] = val;
                    // Update parents
                    size_t p = pos;
                    do {
                        p--; p>>=1;
                        heap[p] = max(heap[2*p+1], heap[2*p+2]);
                    } while (p);

                    // Maybe write out max
                    if (x-radius > 0)
                        im(x-radius, y, t, c) = heap[0];
                    // Update position in circular buffer
                    pos++;
                    if (pos == heap.size()) pos = 2*radius;
                }
            }
        }

        for (int x = 0; x < im.width; x++) {
            for (int c = 0; c < im.channels; c++) {
                // Initialize the heap to contain all inf
                std::fill(heap.begin(), heap.end(), -INF);
                size_t pos = 2*radius;
                for (int y = 0; y < im.height + radius; y++) {                                        
                    float val;
                    if (y < im.height) val = im(x, y, t, c);
                    else val = -INF;
                    // Stuff it in the heap                    
                    heap[pos] = val;
                    // Update parents
                    size_t p = pos;
                    do {
                        p--; p>>=1;
                        heap[p] = max(heap[2*p+1], heap[2*p+2]);
                    } while (p);
                    // write out max
                    if (y-radius > 0) 
                        im(x, y-radius, t, c) = heap[0];
                    // update position in circular buffer
                    pos++;
                    if (pos == heap.size()) pos = 2*radius;
                }
            }
        }
    }

}


void MedianFilter::help() {
    pprintf("-medianfilter applies a median filter with a circular support. The " 
            "sole argument is the pixel radius of the filter.\n"
            "\n"
            "Usage: ImageStack -load input.jpg -medianfilter 10 -save output.jpg\n");
}

void MedianFilter::parse(vector<string> args) {
    assert(args.size() == 1, "-medianfilter takes one argument\n");
    int radius = readInt(args[0]);
    assert(radius > -1, "radius must be positive");
    NewImage im = apply(stack(0), radius);
    pop();
    push(im);
}

NewImage MedianFilter::apply(NewImage im, int radius) {
    return PercentileFilter::apply(im, radius, 0.5);
}

void PercentileFilter::help() {
    printf("-percentilefilter selects a given statistical percentile over a circular support\n"
           "around each pixel. The two arguments are the support radius, and the percentile.\n"
           "A percentile argument of 0.5 gives a median filter, whereas 0 or 1 give min or\n"
           "max filters.\n\n"
           "Usage: ImageStack -load input.jpg -percentilefilter 10 0.25 -save dark.jpg\n\n");
}

void PercentileFilter::parse(vector<string> args) {
    assert(args.size() == 2, "-percentilefilter takes two arguments\n");
    int radius = readInt(args[0]);
    float percentile = readFloat(args[1]);
    assert(0 <= percentile && percentile <= 1, "percentile must be between zero and one");
    if (percentile == 1) { percentile = 0.999; }
    assert(radius > -1, "radius must be positive");
    NewImage im = apply(stack(0), radius, percentile);
    pop();
    push(im);
}

NewImage PercentileFilter::apply(NewImage im, int radius, float percentile) {
    struct SlidingNewImage {

        // We'll use a pair of heap-like data structures, with a circular
        // buffer as the leaves. The internal nodes point to the smaller
        // or greater child. Each node in the buffer belongs to at most
        // one of the two heaps at any given time.
        
        // Buffer to contain pixel values
        vector<float> buf;
 
        // The pair represents:
        // 1) Index in the circular buffer of the value at this node
        // 2) How many valid children this node has. If zero, then 1) is meaningless.
        vector<pair<int, int> > minHeap, maxHeap;

        SlidingNewImage(int maxKey) {
            buf.resize(maxKey);
            size_t heapSize = 1;
            while (heapSize < 2*buf.size()-1) {
                // Add a new level
                heapSize += heapSize+1;
            }
            minHeap.resize(heapSize);
            maxHeap.resize(heapSize);

            for (size_t i = 0; i < heapSize; i++) {
                minHeap[i].first = 0;
                minHeap[i].second = 0;
                maxHeap[i].first = 0;
                maxHeap[i].second = 0;
            }

            // Set the initial pointers at the leaves
            for (size_t i = 0; i < buf.size(); i++) {
                minHeap[i+buf.size()-1].first = i;
                maxHeap[i+buf.size()-1].first = i;
            }
        }

        void insert(int key, float val) {
            float p = pivot();
            buf[key] = val;
            int heapIdx = key + buf.size() - 1;            
            if (isEmpty() || val < p) {
                // add to the max heap
                maxHeap[heapIdx].second = 1;
                minHeap[heapIdx].second = 0;
            } else {
                // add to the min heap
                maxHeap[heapIdx].second = 0;
                minHeap[heapIdx].second = 1;
            }
            // Fix the heaps
            updateFrom(heapIdx);
        }

        void updateFrom(int pos) {
            // walk up both heaps from the same leaf fixing pointers
            int p = pos;
            while (p) {
                // Move to the parent
                p = (p-1)/2;

                // Examine both children, and update the parent accordingly
                pair<int, int> a = minHeap[p*2+1];
                pair<int, int> b = minHeap[p*2+2];
                pair<int, int> parent;
                parent.second = a.second + b.second;
                if (a.second && b.second) {
                    parent.first = (buf[a.first] < buf[b.first]) ? a.first : b.first;
                } else if (b.second) {
                    parent.first = b.first;
                } else {
                    parent.first = a.first;
                }
                if (minHeap[p] == parent) break;
                minHeap[p] = parent;
            }
                 
            p = pos;
            while (p) {
                p = (p-1)/2;
                pair<int, int> a = maxHeap[p*2+1];
                pair<int, int> b = maxHeap[p*2+2];
                pair<int, int> parent;
                parent.second = a.second + b.second;
                if (a.second && b.second) {
                    parent.first = (buf[a.first] > buf[b.first]) ? a.first : b.first;
                } else if (b.second) {
                    parent.first = b.first;
                } else {
                    parent.first = a.first;
                }
                if (maxHeap[p] == parent) break;
                maxHeap[p] = parent;
            }
        }

        void remove(int key) {
            int heapIdx = key+buf.size()-1;
            minHeap[heapIdx].second = 0;
            maxHeap[heapIdx].second = 0;
            updateFrom(heapIdx);
        }

        void rebalance(float percentile) {
            int total = maxHeap[0].second + minHeap[0].second;

            int desiredMinHeapSize = clamp(int(total * (1.0f - percentile)), 0, total-1);

            // Make sure there aren't too few things in the maxHeap
            while (minHeap[0].second > desiredMinHeapSize) {
                // switch the smallest thing in the minHeap into the maxHeap
                int heapIdx = minHeap[0].first + (buf.size()-1);
                minHeap[heapIdx].second = 0;
                maxHeap[heapIdx].second = 1;
                updateFrom(heapIdx);
            }

            // Make sure there aren't too many things in the maxHeap
            while (minHeap[0].second < desiredMinHeapSize) {
                // Switch the largest thing in the maxHeap into the minHeap
                int heapIdx = maxHeap[0].first + (buf.size()-1);
                minHeap[heapIdx].second = 1;
                maxHeap[heapIdx].second = 0;
                updateFrom(heapIdx);                
            }
        }
        
        bool isEmpty() {
            return ((maxHeap[0].second + minHeap[0].second) == 0);
        }

        float pivot() {
            return buf[maxHeap[0].first];
        }

        void debug() {
            int heapSize = minHeap.size();
            printf("min heap:\n");            
            for (int sz = heapSize+1; sz > 1; sz /= 2) {
                for (int i = sz/2-1; i < sz-1; i++) {
                    pair<int, int> node = minHeap[i];
                    if (node.second)
                        printf("%02d ", (int)(buf[node.first]*100));
                    else
                        printf("-- ");                    
                }
                printf("\n");
            }
            printf("max heap:\n");            
            for (int sz = heapSize+1; sz > 1; sz /= 2) {
                for (int i = sz/2-1; i < sz-1; i++) {
                    pair<int, int> node = maxHeap[i];
                    if (node.second)
                        printf("%02d ", (int)(buf[node.first]*100));
                    else
                        printf("-- ");                    
                }
                printf("\n");
            }
        }

    };

    NewImage out(im.width, im.height, im.frames, im.channels);

    // make the filter edge profile
    int d = 2*radius+1;
    vector<int> edge(d);

    for (int i = 0; i < d; i++) {
        edge[i] = (int)(sqrtf(radius*radius - (i - radius)*(i-radius)) + 0.0001f);
    }

    for (int c = 0; c < im.channels; c++) {
        for (int t = 0; t < im.frames; t++) {
            for (int y = 0; y < im.height; y++) {
                // initialize the sliding window for this scanline
                SlidingNewImage window(d*d);
                for (int i = 0; i < 2*radius+1; i++) {
                    int xoff = edge[i];
                    int yoff = i - radius;

                    if (y + yoff >= im.height) { break; }
                    if (y + yoff < 0) { continue; }

                    for (int j = 0; j <= xoff; j++) {
                        if (j >= im.width) { break; }
                        float val = im(j, y+yoff, t, c);
                        window.insert(i*d + j, val);
                    }
                }

                for (int x = 0; x < im.width; x++) {
                    window.rebalance(percentile);
                    //window.debug();

                    out(x, y, t, c) = window.pivot();

                    // move the support one to the right
                    for (int i = 0; i < radius*2+1; i++) {
                        int xoff = edge[i];
                        int yoff = i - radius;

                        if (y + yoff >= im.height) { break; }
                        if (y + yoff < 0) { continue; }

                        // subtract old value
                        if (x - xoff >= 0) {
                            window.remove(i*d + (x-xoff)%d);
                        }

                        // add new value
                        if (x + xoff + 1 < im.width) {
                            float val = im(x+xoff+1, y+yoff, t, c);
                            window.insert(i*d + (x+xoff+1)%d, val);
                        }
                    }
                }
            }
        }
    }

    return out;
}



void CircularFilter::help() {
  pprintf("-circularfilter convolves the image with a uniform circular kernel. It"
          "is a good approximation to out-of-focus blur. The sole argument is the"
          "radius of the filter.\n"
          "\n"
          "Usage: ImageStack -load in.jpg -circularfilter 10 -save out.jpg\n\n");
}

void CircularFilter::parse(vector<string> args) {
    assert(args.size() == 1, "-circularfilter takes one argument\n");

    NewImage im = apply(stack(0), readInt(args[0]));
    pop();
    push(im);
}

NewImage CircularFilter::apply(NewImage im, int radius) {
    NewImage out(im.width, im.height, im.frames, im.channels);

    // maintain the average response currently under the filter, and the number of pixels under the filter
    float average = 0;
    int count = 0;

    // make the filter edge profile
    vector<int> edge(radius*2+1);
    for (int i = 0; i < 2*radius+1; i++) {
        edge[i] = (int)(sqrtf(radius*radius - (i - radius)*(i-radius)) + 0.0001f);
    }

    // figure out the filter area
    for (int i = 0; i < 2*radius+1; i++) {
        count += edge[i]*2+1;
    }

    float invArea = 1.0f/count;

    for (int c = 0; c < im.channels; c++) {
        for (int t = 0; t < im.frames; t++) {
            for (int y = 0; y < im.height; y++) {
                average = 0;
                // initialize the average and count
                for (int i = 0; i < 2*radius+1; i++) {
                    int xoff = edge[i];
                    int yoff = i - radius;
                    int realY = clamp(y + yoff, 0, im.height-1);

                    for (int x = -xoff; x <= xoff; x++) {
                        int realX = clamp(x, 0, im.width-1);
                        float val = im(realX, realY, t, c);
                        average += val;
                    }
                }

                for (int x = 0; x < im.width; x++) {
                    out(x, y, t, c) = average * invArea;

                    // move the histogram to the right
                    for (int i = 0; i < radius*2+1; i++) {
                        int realXOld = max(0, x-edge[i]);
                        int realXNew = min(x+edge[i]+1, im.width-1);
                        int realY = clamp(y+i-radius, 0, im.height-1);

                        // add new value, subtract old value
                        average += im(realXNew, realY, t, c);
                        average -= im(realXOld, realY, t, c);
                    }
                }
            }
        }
    }

    return out;
}



void Envelope::help() {
    pprintf("-envelope computes a lower or upper envelope of the input, which is"
            " smooth, and less than (or greater than) the input. The first argument"
            " should be \"lower\" or \"upper\". The second argument is the desired"
            " smoothness, which is roughly proportional to the pixel radius of a blur.\n"
            "\n"
            "Usage: ImageStack -load a.jpg -envelope upper 50 -display\n");
}

void Envelope::parse(vector<string> args) {
    assert(args.size() == 2, "-envelope takes two arguments\n");
    Mode m;
    if (args[0] == "lower") { m = Lower; }
    else if (args[0] == "upper") { m = Upper; }
    else { panic("Unknown mode: %s. Must be lower or upper.\n", args[0].c_str()); }

    apply(stack(0), m, readInt(args[1]));
}

void Envelope::apply(NewImage im, Mode m, int radius) {
    if (m == Upper) {
        MaxFilter::apply(im, radius);
        RectFilter::apply(im, 2*radius+1, 2*radius+1, 1);
        radius = (radius+2)/3;
        MaxFilter::apply(im, radius);
        RectFilter::apply(im, 2*radius+1, 2*radius+1, 1);  
    }

    if (m == Lower) {
        MinFilter::apply(im, radius);
        RectFilter::apply(im, 2*radius+1, 2*radius+1, 1);
        radius = (radius+2)/3;
        MinFilter::apply(im, radius);
        RectFilter::apply(im, 2*radius+1, 2*radius+1, 1);            
    }
}

void HotPixelSuppression::help() {
    pprintf("-hotpixelsuppression removes salt-and-pepper noise from an image by"
            " constraining each pixel to be within the bounds of its four"
            " neighbors\n\n"
            "Usage: ImageStack -load noisy.jpg -hotpixelsuppression -save denoised.jpg\n");
}


void HotPixelSuppression::parse(vector<string> args) {
    assert(args.size() == 0, 
           "-hotpixelsuppression takes no arguments\n");
    NewImage im = apply(stack(0));
    pop();
    push(im);
}

NewImage HotPixelSuppression::apply(NewImage im) {
    NewImage out(im.width, im.height, im.frames, im.channels);

    for (int t = 0; t < im.frames; t++) {
        for (int y = 1; y < im.height-1; y++) {
            for (int x = 1; x < im.width-1; x++) {
                for (int c = 0; c < im.channels; c++) {
                    float n1 = im(x-1, y, t, c);
                    float n2 = im(x+1, y, t, c);
                    float n3 = im(x, y-1, t, c);
                    float n4 = im(x, y+1, t, c);
                    float here = im(x, y, t, c);
                    float maxn = max(max(n1, n2), max(n3, n4));
                    float minn = min(min(n1, n2), min(n3, n4));
                    if (here > maxn) here = maxn;
                    if (here < minn) here = minn;
                    out(x, y, t, c) = here;
                }
            }
        }
    }

    return out;
}

#include "footer.h"