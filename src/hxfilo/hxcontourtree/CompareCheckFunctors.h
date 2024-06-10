#ifndef COMPARECHECKFUNCTORS_H
#define COMPARECHECKFUNCTORS_H


#include <utility>
#include <cstddef>
#include <array>




template <typename T>
class IndexValueComparator
{
public:
    IndexValueComparator(const IndexValueAbstractBaseComparator<T>* const abstractBaseComparator) : m_abstractBaseComparator{abstractBaseComparator} {}

    inline bool operator ()(const std::pair<T, std::pair<size_t, T> >& a, const std::pair<T, std::pair<size_t, T> >& b) const { return m_abstractBaseComparator->operator ()(a.first, b.first); }
    inline bool operator ()(const std::pair<size_t, T>& a, const std::pair<size_t, T>& b) const { return m_abstractBaseComparator->operator ()(a, b); }
    inline bool operator ()(const T a, const T b) const { return m_abstractBaseComparator->operator ()(a, b); }

private:
    const IndexValueAbstractBaseComparator<T>* const m_abstractBaseComparator;
};


template <typename T>
class IndexValueInvertedComparator
{
public:
    IndexValueInvertedComparator(const IndexValueAbstractBaseComparator<T>* const abstractBaseComparator) : m_abstractBaseComparator{abstractBaseComparator} {}

    inline bool operator ()(const std::pair<T, std::pair<size_t, T> >& a, const std::pair<T, std::pair<size_t, T> >& b) const { return m_abstractBaseComparator->operator ()(b.first, a.first); }
    inline bool operator ()(const std::pair<size_t, T>& a, const std::pair<size_t, T>& b) const { return m_abstractBaseComparator->operator ()(b, a); }
    inline bool operator ()(const T a, const T b) const { return m_abstractBaseComparator->operator ()(b, a); }

private:
    const IndexValueAbstractBaseComparator<T>* const m_abstractBaseComparator;
};




template <typename T>
struct IndexValueAbstractBaseComparator
{
    virtual ~IndexValueAbstractBaseComparator() {}

    virtual bool operator ()(const std::pair<size_t, T>& a, const std::pair<size_t, T>& b) const = 0;
    virtual bool operator ()(const T a, const T b) const = 0;
};


template <typename T>
struct IndexValueSmallerComparator : IndexValueAbstractBaseComparator<T>
{
    inline bool operator ()(const std::pair<size_t, T>& a, const std::pair<size_t, T>& b) const override
    {
        if(a.second != b.second)
            return a.second < b.second;
        else
            return a.first < b.first;
    }

    inline bool operator ()(const T a, const T b) const override { return a < b; }
};


template <typename T>
struct IndexValueGreaterComparator : IndexValueAbstractBaseComparator<T>
{
    inline bool operator ()(const std::pair<size_t, T>& a, const std::pair<size_t, T>& b) const override
    {
        if(a.second != b.second)
            return a.second > b.second;
        else
            return a.first > b.first;
    }

    inline bool operator ()(T a, T b) const override { return a > b; }
};




template <typename T>
class OutOfRangeChecker
{
public:
    OutOfRangeChecker(const float rangeMin, const float rangeMax) : m_rangeMin{rangeMin}, m_rangeMax{rangeMax} {}
    inline bool operator ()(const T value) const { return static_cast<float>(value) < m_rangeMin || static_cast<float>(value) > m_rangeMax; }

private:
    const float m_rangeMin;
    const float m_rangeMax;
};




#endif // COMPARECHECKFUNCTORS_H
