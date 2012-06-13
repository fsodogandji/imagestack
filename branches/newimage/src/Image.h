#ifndef IMAGESTACK_IMAGE_H
#define IMAGESTACK_IMAGE_H

#include "Func.h"

#include "tables.h"
#include "header.h"

// The image data type.
// It's a reference-counted pointer type.

// Note that const Image means that the reference doesn't change, not
// that the pixel data doesn't. It's equivalent to "float * const
// foo", not "const float * foo". This means that methods tagged const
// are those which do not change the metadata, not those which do not
// change the pixel data. 
//
// However, I don't expect people to use const Image everywhere, so I
// also marked all the metadata as const internally, so that gcc lifts
// it out of inner loops. The only method that is non-const is
// operator=(const Image &other), because this is the only way to
// change the metadata.

class Image {
  public:

    const int width, height, frames, channels;
    const int ystride, tstride, cstride;   

    Image() : 
	width(0), height(0), frames(0), channels(0), 
	ystride(0), tstride(0), cstride(0), data(), base(NULL) {
    }

    Image(int w, int h, int f, int c) :
	width(w), height(h), frames(f), channels(c), 
	ystride(w), tstride(w*h), cstride(w*h*f), 
	data(new vector<float>(w*h*f*c+7)), base(compute_base(data)) {
    }

    inline float &operator()(int x, int y) const {
	return (*this)(x, y, 0, 0);
    }

    inline float &operator()(int x, int y, int c) const {
	return (*this)(x, y, 0, c);
    }

    inline float &operator()(int x, int y, int t, int c) const {
	#ifdef BOUNDS_CHECKING
	assert(x >= 0 && x < width &&
	       y >= 0 && y < height &&
	       t >= 0 && t < frames &&
	       c >= 0 && c < channels, 
	       "Access out of bounds: %d %d %d %d\n", 
	       x, y, t, c);
	#endif
        return (((base + c*cstride) + t*tstride) + y*ystride)[x];
    }

    float *baseAddress() const {
        return base;
    }

    Image copy() const {
        Image m(width, height, frames, channels);
	m.set(*this);
        return m;
    }

    const Image region(int x, int y, int t, int c,
		       int xs, int ys, int ts, int cs) const {
	return Image(*this, x, y, t, c, xs, ys, ts, cs);
    }

    const Image column(int x) const {
        return region(x, 0, 0, 0, 1, height, frames, channels);
    }

    const Image row(int y) const {
        return region(0, y, 0, 0, width, 1, frames, channels);
    }

    const Image frame(int t) const {
        return region(0, 0, t, 0, width, height, 1, channels);
    }
    
    const Image channel(int c) const {
        return region(0, 0, 0, c, width, height, frames, 1);
    }

    bool dense() const {
        return (cstride == width*height*frames && tstride == width*height && ystride == width);
    }
   

    bool defined() const {
	return base != NULL;
    }

    bool operator==(const Image &other) const {
	return (base == other.base &&
		ystride == other.ystride &&
		tstride == other.tstride &&
		cstride == other.cstride &&
		width == other.width &&
		height == other.height &&
		frames == other.frames &&
		channels == other.channels);
    }

    bool operator!=(const Image &other) const {
	return !(*this == other);
    }

    void operator+=(const float f) const {
	set((*this) + f);
    }

    void operator*=(const float f) const {
	set((*this) * f);
    }

    void operator-=(const float f) const {
	set((*this) - f);
    }

    void operator/=(const float f) const {
	set((*this) / f);
    }

    void operator+=(const vector<float> f) const {	
	for (int c = 0; c < channels; c++) {
	    channel(c) += f[c % f.size()];
	}
    }

    void operator*=(const vector<float> f) const {
	for (int c = 0; c < channels; c++) {
	    channel(c) *= f[c % f.size()];
	}
    }

    void operator-=(const vector<float> f) const {	
	for (int c = 0; c < channels; c++) {
	    channel(c) -= f[c % f.size()];
	}
    }

    void operator/=(const vector<float> f) const {
	for (int c = 0; c < channels; c++) {
	    channel(c) /= f[c % f.size()];
	}
    }

    template<typename T, typename Enable = typename T::Func>
    void operator+=(const T other) const {
	set((*this) + other);
    }

    template<typename T, typename Enable = typename T::Func>
    void operator*=(const T other) const {
	set((*this) * other);
    }

    template<typename T, typename Enable = typename T::Func>
    void operator-=(const T other) const {
	set((*this) - other);
    }

    template<typename T, typename Enable = typename T::Func>
    void operator/=(const T other) const {
	set((*this) / other);
    }

    typedef enum {ZERO = 0, NEUMANN} BoundaryCondition;

    void sample2D(float fx, float fy, int t, vector<float> &result, BoundaryCondition boundary = ZERO) const {
        int ix = (int)fx;
        int iy = (int)fy;
        const int LEFT = -2;
        const int RIGHT = 3;
        const int WIDTH = 6;
        int minX = ix + LEFT;
        int maxX = ix + RIGHT;
        int minY = iy + LEFT;
        int maxY = iy + RIGHT;

        float weightX[WIDTH];
        float weightY[WIDTH];
        float totalXWeight = 0, totalYWeight = 0;
        for (int x = 0; x < WIDTH; x++) {
            float diff = (fx - (x + ix + LEFT)); // ranges between +/- RIGHT
            float val = lanczos_3(diff);
            weightX[x] = val;
            totalXWeight += val;
        }

        for (int y = 0; y < WIDTH; y++) {
            float diff = (fy - (y + iy + LEFT)); // ranges between +/- RIGHT
            float val = lanczos_3(diff);
            weightY[y] = val;
            totalYWeight += val;
        }

        totalXWeight = 1.0f/totalXWeight;
        totalYWeight = 1.0f/totalYWeight;

        for (int i = 0; i < WIDTH; i++) {
            weightX[i] *= totalXWeight;
            weightY[i] *= totalYWeight;
        }

        for (int c = 0; c < channels; c++) {
            result[c] = 0;
        }

        if (boundary == NEUMANN) {

            float *yWeightPtr = weightY;
            for (int y = minY; y <= maxY; y++) {
                float *xWeightPtr = weightX;
                int sampleY = clamp(0, y, height-1);
                for (int x = minX; x <= maxX; x++) {
                    int sampleX = clamp(0, x, width-1);
                    float yxWeight = (*yWeightPtr) * (*xWeightPtr);
                    for (int c = 0; c < channels; c++) {
			result[c] += (*this)(sampleX, sampleY, t, c) * yxWeight;
                    }
                    xWeightPtr++;
                }
                yWeightPtr++;
            }
        } else {
            float *weightYBase = weightY;
            float *weightXBase = weightX;
            if (minY < 0) {
                weightYBase -= minY;
                minY = 0;
            }
            if (minX < 0) {
                weightXBase -= minX;
                minX = 0;
            }
            if (maxX > width-1) { maxX = width-1; }
            if (maxY > height-1) { maxY = height-1; }
            float *yWeightPtr = weightYBase;
            for (int y = minY; y <= maxY; y++) {
                float *xWeightPtr = weightXBase;
                for (int x = minX; x <= maxX; x++) {
                    float yxWeight = (*yWeightPtr) * (*xWeightPtr);
                    for (int c = 0; c < channels; c++) {
                        result[c] += (*this)(x, y, t, c) * yxWeight;
                    }
                    xWeightPtr++;
                }
                yWeightPtr++;
            }

        }
    }

    void sample2D(float fx, float fy, vector<float> &result) const {
        sample2D(fx, fy, 0, result);
    }


    void sample2DLinear(float fx, float fy, vector<float> &result) const {
        sample2DLinear(fx, fy, 0, result);
    }

    void sample2DLinear(float fx, float fy, int t, vector<float> &result) const {
        int ix = (int)fx;
        int iy = (int)fy;
        fx -= ix;
        fy -= iy;

        for (int c = 0; c < channels; c++) {
            float s1 = (1-fx) * (*this)(ix, iy, t, c) + fx * (*this)(ix+1, iy, t, c);
            float s2 = (1-fx) * (*this)(ix, iy+1, t, c) + fx * (*this)(ix+1, iy+1, t, c);
            result[c] = (1-fy) * s1 + fy * s2;
        }

    }

    void sample3DLinear(float fx, float fy, float ft, vector<float> &result) const {
        int ix = (int)fx;
        int iy = (int)fy;
        int it = (int)ft;
        fx -= ix;
        fy -= iy;
        ft -= it;

        for (int c = 0; c < channels; c++) {
            float s11 = (1-fx) * (*this)(ix, iy, it, c) + fx * (*this)(ix+1, iy, it, c);
	    float s12 = (1-fx) * (*this)(ix, iy+1, it, c) + fx * (*this)(ix+1, iy+1, it, c);
            float s1 = (1-fy) * s11 + fy * s12;

            float s21 = (1-fx) * (*this)(ix, iy, it+1, c) + fx * (*this)(ix+1, iy, it+1, c);
            float s22 = (1-fx) * (*this)(ix, iy+1, it+1, c) + fx * (*this)(ix+1, iy+1, it+1, c);
            float s2 = (1-fy) * s21 + fy * s22;

            result[c] = (1-ft) * s1 + ft * s2;
        }

    }

    void sample3D(float fx, float fy, float ft, vector<float> &result, BoundaryCondition boundary = ZERO) const {
        int ix = (int)fx;
        int iy = (int)fy;
        int it = (int)ft;
        const int LEFT = -2;
        const int RIGHT = 3;
        const int WIDTH = 6;
        int minX = ix + LEFT;
        int maxX = ix + RIGHT;
        int minY = iy + LEFT;
        int maxY = iy + RIGHT;
        int minT = it + LEFT;
        int maxT = it + RIGHT;
        float weightX[WIDTH];
        float weightY[WIDTH];
        float weightT[WIDTH];

        float totalXWeight = 0, totalYWeight = 0, totalTWeight = 0;

        for (int x = 0; x < WIDTH; x++) {
            float diff = (fx - (x + ix + LEFT)); // ranges between +/- RIGHT
            float val = lanczos_3(diff);
            weightX[x] = val;
            totalXWeight += val;
        }

        for (int y = 0; y < WIDTH; y++) {
            float diff = (fy - (y + iy + LEFT)); // ranges between +/- RIGHT
            float val = lanczos_3(diff);
            weightY[y] = val;
            totalYWeight += val;
        }

        for (int t = 0; t < WIDTH; t++) {
            float diff = (ft - (t + it + LEFT)); // ranges between +/- RIGHT
            float val = lanczos_3(diff);
            weightT[t] = val;
            totalTWeight += val;
        }

        totalXWeight = 1.0f/totalXWeight;
        totalYWeight = 1.0f/totalYWeight;
        totalTWeight = 1.0f/totalTWeight;

        for (int i = 0; i < WIDTH; i++) {
            weightX[i] *= totalXWeight;
            weightY[i] *= totalYWeight;
            weightT[i] *= totalTWeight;
        }

        for (int c = 0; c < channels; c++) {
            result[c] = 0;
        }

        if (boundary == NEUMANN) {

            float *tWeightPtr = weightT;
            for (int t = minT; t <= maxT; t++) {
                int sampleT = clamp(t, 0, frames-1);
                float *yWeightPtr = weightY;
                for (int y = minY; y <= maxY; y++) {
                    int sampleY = clamp(y, 0, height-1);
                    float tyWeight = (*yWeightPtr) * (*tWeightPtr);
                    float *xWeightPtr = weightX;
                    for (int x = minX; x <= maxX; x++) {
                        int sampleX = clamp(x, 0, width-1);
                        float tyxWeight = tyWeight * (*xWeightPtr);
                        for (int c = 0; c < channels; c++) {
                            result[c] += (*this)(sampleX, sampleY, sampleT, c) * tyxWeight;
                        }
                        xWeightPtr++;
                    }
                    yWeightPtr++;
                }
                tWeightPtr++;
            }

        } else {

            float *weightTBase = weightT;
            float *weightYBase = weightY;
            float *weightXBase = weightX;

            if (minY < 0) {
                weightYBase -= minY;
                minY = 0;
            }
            if (minX < 0) {
                weightXBase -= minX;
                minX = 0;
            }
            if (minT < 0) {
                weightTBase -= minT;
                minT = 0;
            }
            if (maxX > width-1) { maxX = width-1; }
            if (maxY > height-1) { maxY = height-1; }
            if (maxT > frames-1) { maxT = frames-1; }

            float *tWeightPtr = weightTBase;
            for (int t = minT; t <= maxT; t++) {
                float *yWeightPtr = weightYBase;
                for (int y = minY; y <= maxY; y++) {
                    float *xWeightPtr = weightXBase;
                    for (int x = minX; x <= maxX; x++) {
                        float yxWeight = (*yWeightPtr) * (*xWeightPtr);
                        for (int c = 0; c < channels; c++) {
                            result[c] += (*this)(x, y, t, c) * yxWeight;
                        }
                        xWeightPtr++;
                    }
                    yWeightPtr++;
                }
                tWeightPtr++;
            }
        }

    }

    // Evaluate a function-like object defined in Func.h
    // The second template argument prevents instantiations from
    // things that don't have a nested ::Func type
    template<typename T, typename Enable = typename T::Func>
    void set(const T func) const {
	if (func.bounded()) {
	    int w = func.getWidth(), h = func.getHeight(),
		f = func.getFrames(), c = func.getChannels();
	    assert(width == w &&
		   height == h &&
		   frames == f &&
		   channels == c,
		   "Can only assign from source of matching size\n");
	} else {
	    assert(defined(),
		   "Can't assign unbounded expression to undefined image\n");
	}

	// 4 or 8-wide vector code, distributed across cores
	#if defined __AVX__ || defined __SSE__
        #ifdef __AVX__
	const int vec_width = 8;
        #else 
	const int vec_width = 4;
        #endif
	if (width > vec_width*2) {
	    for (int c = 0; c < channels; c++) {
		for (int t = 0; t < frames; t++) {

		    #ifdef _OPENMP
                    #pragma omp parallel for
		    #endif
		    for (int y = 0; y < height; y++) {
			const int w = width;
			const typename T::Iter iter = func.scanline(y, t, c);
			float * const dst = base + c*cstride + t*tstride + y*ystride;

			// warm up
			int x = 0;			
			while ((size_t)(dst+x) & (vec_width*sizeof(float) - 1)) {
			    dst[x] = iter[x];
			    x++;
			}
			// vectorized steady-state
			while (x < (w-(vec_width-1))) {
			    // Stream is often counterproductive.
			    //_mm256_stream_ps(dst+x, iter.vec(x));
			    *((ImageStack::Func::vec_type *)(dst + x)) = iter.vec(x);
			    x += vec_width;
			}
			// wind down
			while (x < w) {
			    dst[x] = iter[x];
			    x++;
			}
		    }
		}
	    }	
	    return;
	}
        #endif	    	

	// Scalar code, distributed across cores
	for (int c = 0; c < channels; c++) {
	    for (int t = 0; t < frames; t++) {
		#ifdef _OPENMP
		#pragma omp parallel for
		#endif		
		for (int y = 0; y < height; y++) {
		    const typename T::Iter src = func.scanline(y, t, c);
		    Iter dst = scanline(y, t, c);
		    for (int x = 0; x < width; x++) {
			dst[x] = src[x];
		    }
		}
	    }
	}
    }
    // An image itself is one such function-like thing
    typedef Image Func;
    bool bounded() const {return true;}
    int getWidth() const {return width;}
    int getHeight() const {return height;}
    int getFrames() const {return frames;}
    int getChannels() const {return channels;}
    struct Iter {
	float * const addr;
	float operator[](int x) const {return addr[x];}
	float &operator[](int x) {return addr[x];}
	#ifdef __AVX__
	__v8sf vec(int x) const {
	    return _mm256_loadu_ps(addr + x);
	}
	#else 
	#ifdef __SSE__
	__v4sf vec(int x) const {
	    return _mm_loadu_ps(addr + x);
	}
	#endif
	#endif
    };
    Iter scanline(int y, int t, int c) const {
	return {base + y*ystride + t*tstride + c*cstride};
    }

    void set(float x) const {
	set(ImageStack::Func::Const(x));
    }

    // Construct an image from a function-like thing
    template<typename T, typename Enable = typename T::Func>    
    Image(const T func) :
	width(0), height(0), frames(0), channels(0), 
	ystride(0), tstride(0), cstride(0), data(), base(NULL) {
	assert(func.bounded(), "Can only construct an image from a bounded expression\n");
	set(func);
    }

    void operator=(const Image &other) {
	//assert(!defined(), "Can't assign to an already-defined image\n");
	const_cast<int *>(&width)[0] = other.width;
	const_cast<int *>(&height)[0] = other.height;
	const_cast<int *>(&frames)[0] = other.frames;
	const_cast<int *>(&channels)[0] = other.channels;
	const_cast<int *>(&ystride)[0] = other.ystride;
	const_cast<int *>(&tstride)[0] = other.tstride;
	const_cast<int *>(&cstride)[0] = other.cstride;
	const_cast<float **>(&base)[0] = other.base;
	const_cast<std::shared_ptr<std::vector<float> > *>(&data)[0] = other.data;
    }
    

  private:

    static float *compute_base(std::shared_ptr<vector<float> > data) {
	float *base = &((*data)[0]);
	while (((size_t)base) & 0x1f) base++;    
	return base;
    }

    // Region constructor
    Image(Image im, int x, int y, int t, int c,
	  int xs, int ys, int ts, int cs) :
	width(xs), height(ys), frames(ts), channels(cs),
	ystride(im.ystride), tstride(im.tstride), cstride(im.cstride),
	data(im.data), base(&im(x, y, t, c)) {	
    }

    const std::shared_ptr<std::vector<float> > data;
    float * const base;
};


#include "footer.h"
#endif