#pragma once
#include "hpr/assets/ModelAsset.h"

class AssimpModelImporter
{
public:
    // Transactional, CPU-only import. A failure leaves output untouched.
    static bool Import(const std::string& path, ModelAsset& output, std::string& error);
};
