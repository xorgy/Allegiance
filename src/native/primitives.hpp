#pragma once
#include "assets.hpp"
namespace Native {
std::shared_ptr<ModelData> makeProjectile();
std::shared_ptr<ModelData> makeWarp(int detail=0);
std::shared_ptr<ModelData> makeStars();
}
