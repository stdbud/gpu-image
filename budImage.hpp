#pragma once

#include <vector>
#include <algorithm>
#include <iostream>
#include <budUtils.hpp>

namespace bud {

template<typename T>
class Image {
public:
	explicit Image(const int width, const int height, const int nrChannels)
		: m_width(width), m_height(height), m_nrChannels(nrChannels) {
		genImageData();
	}

	virtual void compute() = 0;

	const int m_width;
	const int m_height;
	const int m_nrChannels;
	std::vector<T> m_data;

	bool validateImageData(const std::vector<T>& got)
	{
		if (got.size() != m_data.size()) return false;
		for (size_t i = 0; i < got.size(); ++i) {
			std::cout << i << "th, expected: " << m_data[i] << ", got: " << got[i] << std::endl;
			//if (std::abs(got[i] - m_data[i]) > epsilon()) return false;
		}
		return true;
	}

private:
	void genImageData()
	{
		m_data.resize(m_width * m_height * m_nrChannels);
		std::for_each(m_data.begin(), m_data.end(), [](auto& data) {
			data = genRandomData<T>(127, 0);
		});
	}

	virtual const T epsilon() = 0;
};

class Imagef : public Image<float> {
public:
	explicit Imagef(const int width, const int height, const int nrChannels)
		: Image<float>(width, height, nrChannels) {}

private:
	const float epsilon() final override { return 0.01f; }
};

class Imageu8 : public Image<uint8_t> {
public:
	explicit Imageu8(const int width, const int height, const int nrChannels)
		: Image<uint8_t>(width, height, nrChannels) {}

private:
	const uint8_t epsilon() final override { return 0; }
};

}
