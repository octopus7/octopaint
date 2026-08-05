#include <octopaint/core/Layer.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace octopaint::core
{
    struct LayerTreeAccess final
    {
        [[nodiscard]] static std::vector<std::unique_ptr<Layer>>& Children(GroupLayer& group) noexcept
        {
            return group.children_;
        }

        [[nodiscard]] static std::vector<std::unique_ptr<Layer>> const& Children(GroupLayer const& group) noexcept
        {
            return group.children_;
        }
    };

    namespace
    {
        using LayerList = std::vector<std::unique_ptr<Layer>>;

        struct LayerLocation final
        {
            LayerList* siblings{};
            std::size_t index{};
            std::optional<LayerId> parent;
        };

        [[nodiscard]] Layer* FindIn(LayerList& layers, LayerId const id) noexcept
        {
            for (auto& layer : layers)
            {
                if (layer->Properties().id == id)
                {
                    return layer.get();
                }

                if (layer->Kind() == LayerKind::Group)
                {
                    auto& children = LayerTreeAccess::Children(static_cast<GroupLayer&>(*layer));
                    if (auto* const found = FindIn(children, id))
                    {
                        return found;
                    }
                }
            }

            return nullptr;
        }

        [[nodiscard]] Layer const* FindIn(LayerList const& layers, LayerId const id) noexcept
        {
            for (auto const& layer : layers)
            {
                if (layer->Properties().id == id)
                {
                    return layer.get();
                }

                if (layer->Kind() == LayerKind::Group)
                {
                    auto const& children = LayerTreeAccess::Children(static_cast<GroupLayer const&>(*layer));
                    if (auto const* const found = FindIn(children, id))
                    {
                        return found;
                    }
                }
            }

            return nullptr;
        }

        [[nodiscard]] std::optional<LayerLocation> LocateIn(
            LayerList& layers,
            LayerId const id,
            std::optional<LayerId> const parent = std::nullopt) noexcept
        {
            for (std::size_t index = 0; index < layers.size(); ++index)
            {
                auto& layer = layers[index];
                if (layer->Properties().id == id)
                {
                    return LayerLocation{ &layers, index, parent };
                }

                if (layer->Kind() == LayerKind::Group)
                {
                    auto& group = static_cast<GroupLayer&>(*layer);
                    auto const found = LocateIn(LayerTreeAccess::Children(group), id, layer->Properties().id);
                    if (found)
                    {
                        return found;
                    }
                }
            }

            return std::nullopt;
        }

        [[nodiscard]] bool ContainsId(Layer const& layer, LayerId const id) noexcept
        {
            if (layer.Properties().id == id)
            {
                return true;
            }

            if (layer.Kind() != LayerKind::Group)
            {
                return false;
            }

            for (auto const& child : LayerTreeAccess::Children(static_cast<GroupLayer const&>(layer)))
            {
                if (ContainsId(*child, id))
                {
                    return true;
                }
            }

            return false;
        }

        void CollectIds(Layer const& layer, std::vector<LayerId>& ids)
        {
            ids.push_back(layer.Properties().id);
            if (layer.Kind() == LayerKind::Group)
            {
                for (auto const& child : LayerTreeAccess::Children(static_cast<GroupLayer const&>(layer)))
                {
                    CollectIds(*child, ids);
                }
            }
        }

        void ValidateIn(
            LayerList const& layers,
            std::vector<LayerId>& seen,
            std::vector<LayerValidationIssue>& issues)
        {
            for (auto const& layer : layers)
            {
                auto const id = layer->Properties().id;
                auto const property_error = ValidateLayerProperties(layer->Properties());
                if (property_error != LayerValidationCode::None)
                {
                    issues.push_back({ property_error, id });
                }

                if (std::find(seen.begin(), seen.end(), id) != seen.end())
                {
                    issues.push_back({ LayerValidationCode::DuplicateId, id });
                }
                else
                {
                    seen.push_back(id);
                }

                if (layer->Kind() == LayerKind::Group)
                {
                    ValidateIn(LayerTreeAccess::Children(static_cast<GroupLayer const&>(*layer)), seen, issues);
                }
            }
        }

        [[nodiscard]] GroupLayer* FindGroup(LayerList& roots, LayerId const id) noexcept
        {
            auto* const layer = FindIn(roots, id);
            if (layer == nullptr || layer->Kind() != LayerKind::Group)
            {
                return nullptr;
            }

            return static_cast<GroupLayer*>(layer);
        }
    }

    std::string_view LayerValidationMessage(LayerValidationCode const code) noexcept
    {
        switch (code)
        {
        case LayerValidationCode::None: return "No validation error.";
        case LayerValidationCode::NullLayer: return "A layer cannot be null.";
        case LayerValidationCode::InvalidId: return "A layer ID must be non-zero.";
        case LayerValidationCode::DuplicateId: return "Every layer ID in a tree must be unique.";
        case LayerValidationCode::EmptyName: return "A layer name cannot be empty.";
        case LayerValidationCode::InvalidOpacity: return "Layer opacity must be finite and between zero and one.";
        case LayerValidationCode::ParentNotFound: return "The requested parent layer does not exist.";
        case LayerValidationCode::ParentIsNotGroup: return "Only a group layer can contain child layers.";
        case LayerValidationCode::LayerNotFound: return "The requested layer does not exist.";
        case LayerValidationCode::IndexOutOfRange: return "The requested layer index is out of range.";
        case LayerValidationCode::CannotParentToSelf: return "A layer cannot be its own parent.";
        case LayerValidationCode::CannotMoveIntoDescendant: return "A group cannot be moved into one of its descendants.";
        }

        return "Unknown layer validation error.";
    }

    LayerValidationCode ValidateLayerProperties(LayerProperties const& properties) noexcept
    {
        if (!properties.id.IsValid())
        {
            return LayerValidationCode::InvalidId;
        }

        if (properties.name_utf8.empty())
        {
            return LayerValidationCode::EmptyName;
        }

        if (!std::isfinite(properties.opacity) || properties.opacity < 0.0F || properties.opacity > 1.0F)
        {
            return LayerValidationCode::InvalidOpacity;
        }

        return LayerValidationCode::None;
    }

    Layer::Layer(LayerProperties properties)
        : properties_(std::move(properties))
    {
        auto const validation = ValidateLayerProperties(properties_);
        if (validation != LayerValidationCode::None)
        {
            throw std::invalid_argument(std::string(LayerValidationMessage(validation)));
        }
    }

    LayerProperties const& Layer::Properties() const noexcept
    {
        return properties_;
    }

    void Layer::Rename(std::string name_utf8)
    {
        if (name_utf8.empty())
        {
            throw std::invalid_argument(std::string(LayerValidationMessage(LayerValidationCode::EmptyName)));
        }

        properties_.name_utf8 = std::move(name_utf8);
    }

    void Layer::SetVisible(bool const visible) noexcept
    {
        properties_.visible = visible;
    }

    void Layer::SetLocked(bool const locked) noexcept
    {
        properties_.locked = locked;
    }

    void Layer::SetAlphaLocked(bool const alpha_locked) noexcept
    {
        properties_.alpha_locked = alpha_locked;
    }

    void Layer::SetOpacity(float const opacity)
    {
        if (!std::isfinite(opacity) || opacity < 0.0F || opacity > 1.0F)
        {
            throw std::invalid_argument(std::string(LayerValidationMessage(LayerValidationCode::InvalidOpacity)));
        }

        properties_.opacity = opacity;
    }

    void Layer::SetBlendMode(BlendMode const blend_mode) noexcept
    {
        properties_.blend_mode = blend_mode;
    }

    RasterLayer::RasterLayer(LayerProperties properties)
        : Layer(std::move(properties))
    {
    }

    RasterLayer::RasterLayer(LayerProperties properties, SparseTileStore tiles)
        : Layer(std::move(properties)), tiles_(std::move(tiles))
    {
    }

    LayerKind RasterLayer::Kind() const noexcept
    {
        return LayerKind::Raster;
    }

    SparseTileStore const& RasterLayer::Tiles() const noexcept
    {
        return tiles_;
    }

    SparseTileStore& RasterLayer::Tiles() noexcept
    {
        return tiles_;
    }

    GroupLayer::GroupLayer(LayerProperties properties)
        : Layer(std::move(properties))
    {
    }

    LayerKind GroupLayer::Kind() const noexcept
    {
        return LayerKind::Group;
    }

    std::span<std::unique_ptr<Layer> const> GroupLayer::Children() const noexcept
    {
        return children_;
    }

    std::span<std::unique_ptr<Layer> const> LayerTree::Roots() const noexcept
    {
        return roots_;
    }

    Layer* LayerTree::Find(LayerId const id) noexcept
    {
        return FindIn(roots_, id);
    }

    Layer const* LayerTree::Find(LayerId const id) const noexcept
    {
        return FindIn(roots_, id);
    }

    LayerMutationResult LayerTree::AppendRoot(std::unique_ptr<Layer> layer)
    {
        return Insert(std::nullopt, roots_.size(), std::move(layer));
    }

    LayerMutationResult LayerTree::AppendChild(LayerId const parent, std::unique_ptr<Layer> layer)
    {
        auto* const group = FindGroup(roots_, parent);
        if (group == nullptr)
        {
            auto const* const existing = Find(parent);
            return { existing == nullptr ? LayerValidationCode::ParentNotFound : LayerValidationCode::ParentIsNotGroup, parent };
        }

        return Insert(parent, group->children_.size(), std::move(layer));
    }

    LayerMutationResult LayerTree::Insert(
        std::optional<LayerId> const parent,
        std::size_t const index,
        std::unique_ptr<Layer> layer)
    {
        if (!layer)
        {
            return { LayerValidationCode::NullLayer, {} };
        }

        LayerList* destination = &roots_;
        if (parent)
        {
            auto* const parent_layer = Find(*parent);
            if (parent_layer == nullptr)
            {
                return { LayerValidationCode::ParentNotFound, *parent };
            }
            if (parent_layer->Kind() != LayerKind::Group)
            {
                return { LayerValidationCode::ParentIsNotGroup, *parent };
            }
            destination = &static_cast<GroupLayer*>(parent_layer)->children_;
        }

        if (index > destination->size())
        {
            return { LayerValidationCode::IndexOutOfRange, layer->Properties().id };
        }

        std::vector<LayerId> incoming_ids;
        CollectIds(*layer, incoming_ids);
        for (std::size_t index_a = 0; index_a < incoming_ids.size(); ++index_a)
        {
            auto const id = incoming_ids[index_a];
            if (!id.IsValid())
            {
                return { LayerValidationCode::InvalidId, id };
            }
            if (Find(id) != nullptr)
            {
                return { LayerValidationCode::DuplicateId, id };
            }
            if (std::find(incoming_ids.begin(), incoming_ids.begin() + static_cast<std::ptrdiff_t>(index_a), id)
                != incoming_ids.begin() + static_cast<std::ptrdiff_t>(index_a))
            {
                return { LayerValidationCode::DuplicateId, id };
            }
        }

        destination->insert(destination->begin() + static_cast<std::ptrdiff_t>(index), std::move(layer));
        return {};
    }

    LayerRemovalResult LayerTree::Remove(LayerId const id)
    {
        auto const location = LocateIn(roots_, id);
        if (!location)
        {
            return { { LayerValidationCode::LayerNotFound, id }, nullptr };
        }

        auto removed = std::move((*location->siblings)[location->index]);
        location->siblings->erase(location->siblings->begin() + static_cast<std::ptrdiff_t>(location->index));
        return { {}, std::move(removed) };
    }

    LayerMutationResult LayerTree::Move(
        LayerId const id,
        std::optional<LayerId> const new_parent,
        std::size_t const final_index)
    {
        auto const source = LocateIn(roots_, id);
        if (!source)
        {
            return { LayerValidationCode::LayerNotFound, id };
        }

        if (new_parent && *new_parent == id)
        {
            return { LayerValidationCode::CannotParentToSelf, id };
        }

        LayerList* destination = &roots_;
        if (new_parent)
        {
            auto* const parent_layer = Find(*new_parent);
            if (parent_layer == nullptr)
            {
                return { LayerValidationCode::ParentNotFound, *new_parent };
            }
            if (parent_layer->Kind() != LayerKind::Group)
            {
                return { LayerValidationCode::ParentIsNotGroup, *new_parent };
            }
            if (ContainsId(*(*source->siblings)[source->index], *new_parent))
            {
                return { LayerValidationCode::CannotMoveIntoDescendant, id };
            }
            destination = &static_cast<GroupLayer*>(parent_layer)->children_;
        }

        auto const same_container = destination == source->siblings;
        auto const final_size = destination->size() - (same_container ? 1U : 0U);
        if (final_index > final_size)
        {
            return { LayerValidationCode::IndexOutOfRange, id };
        }

        auto moving = std::move((*source->siblings)[source->index]);
        source->siblings->erase(source->siblings->begin() + static_cast<std::ptrdiff_t>(source->index));
        destination->insert(destination->begin() + static_cast<std::ptrdiff_t>(final_index), std::move(moving));
        return {};
    }

    std::vector<LayerValidationIssue> LayerTree::Validate() const
    {
        std::vector<LayerValidationIssue> issues;
        std::vector<LayerId> seen;
        ValidateIn(roots_, seen, issues);
        return issues;
    }
}
