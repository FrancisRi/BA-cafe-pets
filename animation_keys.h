#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace nativepet {
// Some exporters round adjacent keys to the same float timestamp. Keep the
// last complete key (including both cubic tangents); never retime the clip or
// merge nearby but distinct keys. Decreasing/non-finite times remain errors.
inline size_t collapseDuplicateKeys(std::vector<float>& times,std::vector<float>& values,size_t stride){
    if(!stride||values.size()/stride!=times.size()||values.size()%stride)
        throw std::runtime_error("Invalid animation sampler dimensions");
    for(size_t i=0;i<times.size();++i)
        if(!std::isfinite(times[i])||times[i]<0||(i&&times[i]<times[i-1]))
            throw std::runtime_error("Animation times must be finite, nonnegative and nondecreasing");
    size_t kept=0;
    for(size_t i=0;i<times.size();++i){
        const bool duplicate=kept&&times[i]==times[kept-1];
        const size_t destination=duplicate?kept-1:kept++;
        times[destination]=times[i];
        if(destination!=i)std::copy_n(values.data()+i*stride,stride,values.data()+destination*stride);
    }
    const auto removed=times.size()-kept;
    times.resize(kept);values.resize(kept*stride);return removed;
}
}
