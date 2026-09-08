#include "hpr/renderer/ModelGpuCache.h"
#include <iostream>
GpuRenderScene ModelGpuCache::prepare(const RenderScene& scene)
{
    for (auto it = entries.begin(); it != entries.end();)
        if (it->first.expired())
            it = entries.erase(it);
        else
            ++it;
    GpuRenderScene prepared;
    prepared.directionalLight = scene.directionalLight;
    prepared.pointLights = scene.pointLights;
    prepared.environment = scene.environment;
    prepared.skybox = scene.skybox;
    prepared.environmentMode = scene.environmentMode;
    for (const auto& item : scene.objects)
    {
        if (!item.model || !item.model->isValid())
            continue;
        auto& entry = entries[Key(item.model)];
        if (!entry.model || entry.revision != item.model->getRevision())
        {
            try
            {
                auto replacement = std::make_shared<GpuModel>(*item.model, loader, item.model->getRevision() > 1);
                if (replacement->isValid())
                {
                    entry.model = std::move(replacement);
                    entry.revision = item.model->getRevision();
                }
            }
            catch (const std::exception& error)
            {
                std::cerr << "GPU model upload failed: " << error.what() << '\n';
            }
        }
        if (entry.model)
            prepared.objects.push_back({entry.model, item.transform, item.material});
    }
    return prepared;
}
