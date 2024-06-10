#ifndef CONVERTVECTORANDMCDARRAY_H
#define CONVERTVECTORANDMCDARRAY_H

#include <vector>
#include <mclib/McDArray.h>

template <typename T>
McDArray<T>
convertVector2McDArray(const std::vector<T>& vec){
    McDArray<T> arr;
    for(T it : vec)
        arr.append(it);

    return arr;
}

template <typename T>
std::vector<T>
convertMcDArray2Vector(const McDArray<T>& arr){
    std::vector<T> vec;
    for(T it : arr)
        vec.push_back(it);

    return vec;
}


#endif // CONVERTVECTORANDMCDARRAY_H
