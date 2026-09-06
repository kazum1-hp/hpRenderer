#include "IBLCache.h"
#include "ResourceManager.h"
#include <iostream>
#include <stdexcept>

void IBLCache::initialize(ResourceManager& resourceManager)
{
    clear();
    resources = &resourceManager;
    precompute.initialize(resourceManager);
}

void IBLCache::clear()
{
    entries.clear();
    precompute.shutdown();
    resources = nullptr;
}

EnvironmentGpuView IBLCache::prepare(const std::shared_ptr<const EnvironmentAsset>& asset)
{
    // The cache must not keep a discarded scene/selection alive.
    for (auto it = entries.begin(); it != entries.end(); )
        if (it->first.expired()) it = entries.erase(it); else ++it;
    if (!asset) return {};
    if (!resources) throw std::logic_error("IBLCache is not initialized");

    auto& entry = entries[Key(asset)];
    auto source = resources->GetEnvironmentTexture(*asset);
    const auto programs = precompute.programIds();
    // Source object identity also catches a generic texture reload and avoids
    // relying on OpenGL names (which may be reused after deletion).
    if (entry.revision != asset->getRevision() || entry.source != source || entry.programs != programs)
    {
        entry.revision = asset->getRevision();
        entry.source = source;
        entry.programs = programs;
        try
        {
            if (!source) throw std::runtime_error("HDR texture could not be loaded");
            auto replacement = precompute.bake(source->getID());
            entry.resources = std::move(replacement);
        }
        catch (const std::exception& error)
        {
            // Retry after a source/program change, not every frame. Keep the
            // last complete maps of THIS asset, never another scene's maps.
            std::cerr << "IBL bake failed for " << asset->getPath() << ": " << error.what() << '\n';
        }
    }
    return entry.resources ? entry.resources->view() : EnvironmentGpuView{};
}
