#include <octopaint/application/Workspace.h>

#include <octopaint/core/Document.h>
#include <octopaint/core/Layer.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
    std::atomic_uint64_t next_document_id{ 1 };
    std::atomic_uint64_t next_layer_id{ 1 };

    [[nodiscard]] std::uint64_t AllocateUniqueId(std::atomic_uint64_t& source, char const* const exhausted_message)
    {
        auto value = source.load(std::memory_order_relaxed);
        for (;;)
        {
            // Leave max as a permanent exhausted sentinel instead of allowing
            // unsigned wraparound to make a stale identity valid again.
            if (value == 0 || value == std::numeric_limits<std::uint64_t>::max())
            {
                throw std::overflow_error(exhausted_message);
            }

            if (source.compare_exchange_weak(
                value,
                value + 1,
                std::memory_order_relaxed,
                std::memory_order_relaxed))
            {
                return value;
            }
        }
    }

    [[nodiscard]] std::uint64_t AllocateDocumentId()
    {
        return AllocateUniqueId(next_document_id, "The process exhausted its document identities.");
    }

    [[nodiscard]] std::uint64_t AllocateLayerId()
    {
        return AllocateUniqueId(next_layer_id, "The process exhausted its layer identities.");
    }
}

namespace octopaint::application::detail
{
    struct LayerDocumentState final
    {
        core::LayerTree tree;
        std::optional<LayerId> active_layer_id;
    };

    struct LayerLocation final
    {
        std::optional<core::LayerId> parent;
        std::size_t index{};
    };

    [[nodiscard]] core::LayerId ToCore(LayerId const id) noexcept
    {
        return core::LayerId{ id.Value() };
    }

    [[nodiscard]] core::Layer* Find(LayerDocumentState& state, LayerId const id) noexcept
    {
        return state.tree.Find(ToCore(id));
    }

    [[nodiscard]] core::Layer const* Find(LayerDocumentState const& state, LayerId const id) noexcept
    {
        return state.tree.Find(ToCore(id));
    }

    [[nodiscard]] std::optional<LayerLocation> LocateIn(
        std::span<std::unique_ptr<core::Layer> const> const layers,
        core::LayerId const id,
        std::optional<core::LayerId> const parent = std::nullopt) noexcept
    {
        for (std::size_t index = 0; index < layers.size(); ++index)
        {
            auto const& layer = layers[index];
            if (layer->Properties().id == id)
            {
                return LayerLocation{ parent, index };
            }
            if (layer->Kind() == core::LayerKind::Group)
            {
                auto const& group = static_cast<core::GroupLayer const&>(*layer);
                if (auto const found = LocateIn(group.Children(), id, layer->Properties().id))
                {
                    return found;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<LayerLocation> Locate(
        LayerDocumentState const& state,
        LayerId const id) noexcept
    {
        return LocateIn(state.tree.Roots(), ToCore(id));
    }

    [[nodiscard]] std::size_t ChildCount(
        LayerDocumentState const& state,
        std::optional<LayerId> const parent)
    {
        if (!parent)
        {
            return state.tree.Roots().size();
        }

        auto const* const layer = Find(state, *parent);
        if (!layer)
        {
            throw std::out_of_range("The parent layer does not exist.");
        }
        if (layer->Kind() != core::LayerKind::Group)
        {
            throw std::invalid_argument("Only a group layer can contain child layers.");
        }
        return static_cast<core::GroupLayer const*>(layer)->Children().size();
    }

    void RequireSuccess(core::LayerMutationResult const result)
    {
        if (!result)
        {
            throw std::invalid_argument(std::string{ core::LayerValidationMessage(result.code) });
        }
    }
}

namespace octopaint::application
{
    struct Workspace::Impl final
    {
        struct HistoryEntry final
        {
            std::unique_ptr<DocumentCommand> command;
            std::uint64_t before_revision{};
            std::uint64_t after_revision{};
        };

        struct OpenDocument final
        {
            DocumentId id;
            core::Document document;
            detail::LayerDocumentState layers;
            std::vector<HistoryEntry> history;
            std::size_t history_position{};
            std::uint64_t current_revision{};
            std::uint64_t saved_revision{};
            std::uint64_t next_revision{ 1 };
        };

        std::vector<OpenDocument> documents;
        std::optional<DocumentId> active_document_id;

        [[nodiscard]] OpenDocument* Find(DocumentId const id) noexcept
        {
            auto const iterator = std::ranges::find(documents, id, &OpenDocument::id);
            return iterator == documents.end() ? nullptr : &*iterator;
        }

        [[nodiscard]] OpenDocument const* Find(DocumentId const id) const noexcept
        {
            auto const iterator = std::ranges::find(documents, id, &OpenDocument::id);
            return iterator == documents.end() ? nullptr : &*iterator;
        }

        [[nodiscard]] OpenDocument* Active() noexcept
        {
            return active_document_id ? Find(*active_document_id) : nullptr;
        }

        [[nodiscard]] OpenDocument const* Active() const noexcept
        {
            return active_document_id ? Find(*active_document_id) : nullptr;
        }
    };

    DocumentMutation::DocumentMutation(void* const document, void* const layers) noexcept
        : document_(document), layers_(layers)
    {
    }

    std::string const& DocumentMutation::Title() const noexcept
    {
        return static_cast<core::Document const*>(document_)->Title();
    }

    CanvasSize DocumentMutation::Size() const noexcept
    {
        auto const size = static_cast<core::Document const*>(document_)->Size();
        return { .width = size.width, .height = size.height };
    }

    void DocumentMutation::Rename(std::string title)
    {
        static_cast<core::Document*>(document_)->Rename(std::move(title));
    }

    RenameDocumentCommand::RenameDocumentCommand(std::string new_title)
        : new_title_(std::move(new_title))
    {
        if (new_title_.empty())
        {
            throw std::invalid_argument("A document title cannot be empty.");
        }
    }

    std::string RenameDocumentCommand::Label() const
    {
        return "Rename document";
    }

    void RenameDocumentCommand::Execute(DocumentMutation& document)
    {
        if (!captured_previous_title_)
        {
            previous_title_ = document.Title();
            captured_previous_title_ = true;
        }

        document.Rename(new_title_);
    }

    void RenameDocumentCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_title_)
        {
            throw std::logic_error("Cannot undo a command that has not executed.");
        }

        document.Rename(previous_title_);
    }

    AddRasterLayerCommand::AddRasterLayerCommand(
        std::string name,
        std::optional<LayerId> parent,
        std::optional<std::size_t> index)
        : id_(LayerId{ AllocateLayerId() }),
          name_(std::move(name)),
          parent_(parent),
          requested_index_(index)
    {
        if (name_.empty())
        {
            throw std::invalid_argument("A layer name cannot be empty.");
        }
    }

    LayerId AddRasterLayerCommand::CreatedLayerId() const noexcept
    {
        return id_;
    }

    std::string AddRasterLayerCommand::Label() const
    {
        return "Add raster layer";
    }

    void AddRasterLayerCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        if (!executed_once_)
        {
            previous_active_layer_ = state.active_layer_id;
            insertion_index_ = requested_index_.value_or(detail::ChildCount(state, parent_));
            executed_once_ = true;
        }

        auto layer = std::make_unique<core::RasterLayer>(core::LayerProperties{
            .id = detail::ToCore(id_),
            .name_utf8 = name_
        });
        detail::RequireSuccess(state.tree.Insert(
            parent_ ? std::optional{ detail::ToCore(*parent_) } : std::nullopt,
            insertion_index_,
            std::move(layer)));
        state.active_layer_id = id_;
    }

    void AddRasterLayerCommand::Undo(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto removed = state.tree.Remove(detail::ToCore(id_));
        detail::RequireSuccess(removed.result);
        state.active_layer_id = previous_active_layer_;
    }

    AddGroupLayerCommand::AddGroupLayerCommand(
        std::string name,
        std::optional<LayerId> parent,
        std::optional<std::size_t> index)
        : id_(LayerId{ AllocateLayerId() }),
          name_(std::move(name)),
          parent_(parent),
          requested_index_(index)
    {
        if (name_.empty())
        {
            throw std::invalid_argument("A layer name cannot be empty.");
        }
    }

    LayerId AddGroupLayerCommand::CreatedLayerId() const noexcept
    {
        return id_;
    }

    std::string AddGroupLayerCommand::Label() const
    {
        return "Add group layer";
    }

    void AddGroupLayerCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        if (!executed_once_)
        {
            previous_active_layer_ = state.active_layer_id;
            insertion_index_ = requested_index_.value_or(detail::ChildCount(state, parent_));
            executed_once_ = true;
        }

        auto layer = std::make_unique<core::GroupLayer>(core::LayerProperties{
            .id = detail::ToCore(id_),
            .name_utf8 = name_
        });
        detail::RequireSuccess(state.tree.Insert(
            parent_ ? std::optional{ detail::ToCore(*parent_) } : std::nullopt,
            insertion_index_,
            std::move(layer)));
        state.active_layer_id = id_;
    }

    void AddGroupLayerCommand::Undo(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto removed = state.tree.Remove(detail::ToCore(id_));
        detail::RequireSuccess(removed.result);
        state.active_layer_id = previous_active_layer_;
    }

    struct RemoveLayerCommand::Impl final
    {
        explicit Impl(LayerId const layer_id)
            : id(layer_id)
        {
        }

        LayerId id;
        std::optional<core::LayerId> parent;
        std::size_t index{};
        std::optional<LayerId> previous_active_layer;
        std::unique_ptr<core::Layer> removed_layer;
        bool executed_once{};
    };

    RemoveLayerCommand::RemoveLayerCommand(LayerId const id)
        : impl_(std::make_unique<Impl>(id))
    {
        if (!id)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
    }

    RemoveLayerCommand::~RemoveLayerCommand() = default;

    std::string RemoveLayerCommand::Label() const
    {
        return "Remove layer";
    }

    void RemoveLayerCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto const location = detail::Locate(state, impl_->id);
        if (!location)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!impl_->executed_once)
        {
            impl_->parent = location->parent;
            impl_->index = location->index;
            impl_->previous_active_layer = state.active_layer_id;
            impl_->executed_once = true;
        }

        auto removal = state.tree.Remove(detail::ToCore(impl_->id));
        detail::RequireSuccess(removal.result);
        impl_->removed_layer = std::move(removal.layer);

        if (state.active_layer_id && !detail::Find(state, *state.active_layer_id))
        {
            if (impl_->parent)
            {
                state.active_layer_id = LayerId{ impl_->parent->Value() };
            }
            else if (!state.tree.Roots().empty())
            {
                state.active_layer_id = LayerId{ state.tree.Roots().front()->Properties().id.Value() };
            }
            else
            {
                state.active_layer_id.reset();
            }
        }
    }

    void RemoveLayerCommand::Undo(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        if (!impl_->removed_layer)
        {
            throw std::logic_error("Cannot undo a layer removal that has not executed.");
        }
        detail::RequireSuccess(state.tree.Insert(
            impl_->parent,
            impl_->index,
            std::move(impl_->removed_layer)));
        state.active_layer_id = impl_->previous_active_layer;
    }

    MoveLayerCommand::MoveLayerCommand(
        LayerId const id,
        std::optional<LayerId> new_parent,
        std::size_t const new_index)
        : id_(id), new_parent_(new_parent), new_index_(new_index)
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
    }

    std::string MoveLayerCommand::Label() const
    {
        return "Move layer";
    }

    void MoveLayerCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        if (!captured_previous_location_)
        {
            auto const location = detail::Locate(state, id_);
            if (!location)
            {
                throw std::out_of_range("The layer does not exist.");
            }
            if (location->parent)
            {
                previous_parent_ = LayerId{ location->parent->Value() };
            }
            previous_index_ = location->index;
            captured_previous_location_ = true;
        }

        detail::RequireSuccess(state.tree.Move(
            detail::ToCore(id_),
            new_parent_ ? std::optional{ detail::ToCore(*new_parent_) } : std::nullopt,
            new_index_));
    }

    void MoveLayerCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_location_)
        {
            throw std::logic_error("Cannot undo a layer move that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        detail::RequireSuccess(state.tree.Move(
            detail::ToCore(id_),
            previous_parent_ ? std::optional{ detail::ToCore(*previous_parent_) } : std::nullopt,
            previous_index_));
    }

    RenameLayerCommand::RenameLayerCommand(LayerId const id, std::string new_name)
        : id_(id), new_name_(std::move(new_name))
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
        if (new_name_.empty())
        {
            throw std::invalid_argument("A layer name cannot be empty.");
        }
    }

    std::string RenameLayerCommand::Label() const
    {
        return "Rename layer";
    }

    void RenameLayerCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!captured_previous_name_)
        {
            previous_name_ = layer->Properties().name_utf8;
            captured_previous_name_ = true;
        }
        layer->Rename(new_name_);
    }

    void RenameLayerCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_name_)
        {
            throw std::logic_error("Cannot undo a layer rename that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        layer->Rename(previous_name_);
    }

    SetLayerOpacityCommand::SetLayerOpacityCommand(LayerId const id, float const opacity)
        : id_(id), opacity_(opacity)
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
        if (!std::isfinite(opacity_) || opacity_ < 0.0F || opacity_ > 1.0F)
        {
            throw std::invalid_argument("Layer opacity must be finite and between zero and one.");
        }
    }

    std::string SetLayerOpacityCommand::Label() const
    {
        return "Set layer opacity";
    }

    void SetLayerOpacityCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!captured_previous_opacity_)
        {
            previous_opacity_ = layer->Properties().opacity;
            captured_previous_opacity_ = true;
        }
        layer->SetOpacity(opacity_);
    }

    void SetLayerOpacityCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_opacity_)
        {
            throw std::logic_error("Cannot undo an opacity command that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        layer->SetOpacity(previous_opacity_);
    }

    SetLayerVisibilityCommand::SetLayerVisibilityCommand(LayerId const id, bool const visible)
        : id_(id), visible_(visible)
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
    }

    std::string SetLayerVisibilityCommand::Label() const
    {
        return "Set layer visibility";
    }

    void SetLayerVisibilityCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!captured_previous_visibility_)
        {
            previous_visibility_ = layer->Properties().visible;
            captured_previous_visibility_ = true;
        }
        layer->SetVisible(visible_);
    }

    void SetLayerVisibilityCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_visibility_)
        {
            throw std::logic_error("Cannot undo a visibility command that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        layer->SetVisible(previous_visibility_);
    }

    SetLayerAlphaLockedCommand::SetLayerAlphaLockedCommand(
        LayerId const id,
        bool const alpha_locked)
        : id_(id), alpha_locked_(alpha_locked)
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
    }

    std::string SetLayerAlphaLockedCommand::Label() const
    {
        return "Set layer alpha lock";
    }

    void SetLayerAlphaLockedCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!captured_previous_alpha_locked_)
        {
            previous_alpha_locked_ = layer->Properties().alpha_locked;
            captured_previous_alpha_locked_ = true;
        }
        layer->SetAlphaLocked(alpha_locked_);
    }

    void SetLayerAlphaLockedCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_alpha_locked_)
        {
            throw std::logic_error("Cannot undo an alpha lock command that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        layer->SetAlphaLocked(previous_alpha_locked_);
    }

    SetLayerBlendModeCommand::SetLayerBlendModeCommand(LayerId const id, BlendMode const blend_mode)
        : id_(id), blend_mode_(blend_mode)
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
    }

    std::string SetLayerBlendModeCommand::Label() const
    {
        return "Set layer blend mode";
    }

    void SetLayerBlendModeCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!captured_previous_blend_mode_)
        {
            previous_blend_mode_ = static_cast<BlendMode>(layer->Properties().blend_mode);
            captured_previous_blend_mode_ = true;
        }
        layer->SetBlendMode(static_cast<core::BlendMode>(blend_mode_));
    }

    void SetLayerBlendModeCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_blend_mode_)
        {
            throw std::logic_error("Cannot undo a blend mode command that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        layer->SetBlendMode(static_cast<core::BlendMode>(previous_blend_mode_));
    }

    Workspace::Workspace()
        : impl_(std::make_unique<Impl>())
    {
    }

    Workspace::~Workspace() = default;
    Workspace::Workspace(Workspace&&) noexcept = default;
    Workspace& Workspace::operator=(Workspace&&) noexcept = default;

    DocumentId Workspace::NewDocument(std::string title, CanvasSize const size)
    {
        if (!size.IsValid())
        {
            throw std::invalid_argument("Canvas dimensions must be greater than zero.");
        }

        DocumentId const id{ AllocateDocumentId() };
        LayerId const default_layer_id{ AllocateLayerId() };
        detail::LayerDocumentState layers;
        detail::RequireSuccess(layers.tree.AppendRoot(std::make_unique<core::RasterLayer>(
            core::LayerProperties{
                .id = detail::ToCore(default_layer_id),
                .name_utf8 = "Layer 1"
            })));
        layers.active_layer_id = default_layer_id;
        impl_->documents.push_back({
            .id = id,
            .document = core::Document{
                std::move(title),
                core::CanvasSize{ .width = size.width, .height = size.height } },
            .layers = std::move(layers)
        });
        impl_->active_document_id = id;
        return id;
    }

    bool Workspace::CloseDocument(DocumentId const id)
    {
        auto const iterator = std::ranges::find(impl_->documents, id, &Impl::OpenDocument::id);
        if (iterator == impl_->documents.end())
        {
            return false;
        }

        bool const was_active = impl_->active_document_id == id;
        auto const index = static_cast<std::size_t>(iterator - impl_->documents.begin());
        impl_->documents.erase(iterator);

        if (was_active)
        {
            if (impl_->documents.empty())
            {
                impl_->active_document_id.reset();
            }
            else
            {
                auto const next_index = std::min(index, impl_->documents.size() - 1);
                impl_->active_document_id = impl_->documents[next_index].id;
            }
        }

        return true;
    }

    bool Workspace::ActivateDocument(DocumentId const id)
    {
        if (!impl_->Find(id))
        {
            return false;
        }

        impl_->active_document_id = id;
        return true;
    }

    std::optional<DocumentId> Workspace::ActiveDocument() const noexcept
    {
        return impl_->active_document_id;
    }

    bool Workspace::Contains(DocumentId const id) const noexcept
    {
        return impl_->Find(id) != nullptr;
    }

    bool Workspace::ActivateLayer(DocumentId const document_id, LayerId const layer_id)
    {
        auto* const open_document = impl_->Find(document_id);
        if (!open_document || !detail::Find(open_document->layers, layer_id))
        {
            return false;
        }
        open_document->layers.active_layer_id = layer_id;
        return true;
    }

    bool Workspace::ActivateLayer(LayerId const layer_id)
    {
        auto* const active = impl_->Active();
        return active ? ActivateLayer(active->id, layer_id) : false;
    }

    void Workspace::ExecuteCommand(
        DocumentId const id,
        std::unique_ptr<DocumentCommand> command)
    {
        auto* const open_document = impl_->Find(id);
        if (!open_document)
        {
            throw std::out_of_range("The document is not open in this workspace.");
        }
        if (!command)
        {
            throw std::invalid_argument("A document command cannot be null.");
        }

        DocumentMutation mutation{ &open_document->document, &open_document->layers };
        command->Execute(mutation);

        if (open_document->history_position < open_document->history.size())
        {
            open_document->history.erase(
                open_document->history.begin()
                    + static_cast<std::ptrdiff_t>(open_document->history_position),
                open_document->history.end());
        }

        auto const before_revision = open_document->current_revision;
        auto const after_revision = open_document->next_revision++;
        open_document->history.push_back({
            .command = std::move(command),
            .before_revision = before_revision,
            .after_revision = after_revision
        });
        open_document->history_position = open_document->history.size();
        open_document->current_revision = after_revision;
    }

    bool Workspace::Undo(DocumentId const id)
    {
        auto* const open_document = impl_->Find(id);
        if (!open_document)
        {
            throw std::out_of_range("The document is not open in this workspace.");
        }
        if (open_document->history_position == 0)
        {
            return false;
        }

        auto& entry = open_document->history[open_document->history_position - 1];
        DocumentMutation mutation{ &open_document->document, &open_document->layers };
        entry.command->Undo(mutation);
        --open_document->history_position;
        open_document->current_revision = entry.before_revision;
        return true;
    }

    bool Workspace::Redo(DocumentId const id)
    {
        auto* const open_document = impl_->Find(id);
        if (!open_document)
        {
            throw std::out_of_range("The document is not open in this workspace.");
        }
        if (open_document->history_position == open_document->history.size())
        {
            return false;
        }

        auto& entry = open_document->history[open_document->history_position];
        DocumentMutation mutation{ &open_document->document, &open_document->layers };
        entry.command->Execute(mutation);
        ++open_document->history_position;
        open_document->current_revision = entry.after_revision;
        return true;
    }

    void Workspace::MarkSaved(DocumentId const id)
    {
        auto* const open_document = impl_->Find(id);
        if (!open_document)
        {
            throw std::out_of_range("The document is not open in this workspace.");
        }

        open_document->saved_revision = open_document->current_revision;
    }

    void Workspace::ExecuteCommand(std::unique_ptr<DocumentCommand> command)
    {
        if (!impl_->active_document_id)
        {
            throw std::logic_error("The workspace has no active document.");
        }
        ExecuteCommand(*impl_->active_document_id, std::move(command));
    }

    bool Workspace::Undo()
    {
        if (!impl_->active_document_id)
        {
            return false;
        }
        return Undo(*impl_->active_document_id);
    }

    bool Workspace::Redo()
    {
        if (!impl_->active_document_id)
        {
            return false;
        }
        return Redo(*impl_->active_document_id);
    }

    void Workspace::MarkActiveDocumentSaved()
    {
        if (!impl_->active_document_id)
        {
            throw std::logic_error("The workspace has no active document.");
        }
        MarkSaved(*impl_->active_document_id);
    }

    WorkspaceSnapshot Workspace::Snapshot() const
    {
        WorkspaceSnapshot snapshot;
        snapshot.documents.reserve(impl_->documents.size());
        snapshot.active_document_id = impl_->active_document_id;

        for (auto const& open_document : impl_->documents)
        {
            CommandAvailability commands;
            commands.can_undo = open_document.history_position > 0;
            commands.can_redo = open_document.history_position < open_document.history.size();
            if (commands.can_undo)
            {
                commands.undo_label = open_document.history[open_document.history_position - 1].command->Label();
            }
            if (commands.can_redo)
            {
                commands.redo_label = open_document.history[open_document.history_position].command->Label();
            }

            auto const size = open_document.document.Size();
            DocumentSummary summary{
                .id = open_document.id,
                .title = open_document.document.Title(),
                .canvas_size = { .width = size.width, .height = size.height },
                .is_active = impl_->active_document_id == open_document.id,
                .is_dirty = open_document.current_revision != open_document.saved_revision,
                .current_revision = open_document.current_revision,
                .saved_revision = open_document.saved_revision,
                .commands = std::move(commands),
                .active_layer_id = open_document.layers.active_layer_id
            };

            auto collect_layers = [&summary](
                auto&& self,
                std::span<std::unique_ptr<core::Layer> const> const layers,
                std::optional<LayerId> const parent,
                std::size_t const depth) -> void
            {
                for (std::size_t index = 0; index < layers.size(); ++index)
                {
                    auto const& layer = *layers[index];
                    auto const& properties = layer.Properties();
                    LayerId const layer_id{ properties.id.Value() };
                    summary.layers.push_back({
                        .id = layer_id,
                        .parent_id = parent,
                        .sibling_index = index,
                        .depth = depth,
                        .kind = static_cast<LayerKind>(layer.Kind()),
                        .name = properties.name_utf8,
                        .visible = properties.visible,
                        .locked = properties.locked,
                        .alpha_locked = properties.alpha_locked,
                        .opacity = properties.opacity,
                        .blend_mode = static_cast<BlendMode>(properties.blend_mode)
                    });

                    if (layer.Kind() == core::LayerKind::Group)
                    {
                        self(
                            self,
                            static_cast<core::GroupLayer const&>(layer).Children(),
                            layer_id,
                            depth + 1);
                    }
                }
            };
            collect_layers(collect_layers, open_document.layers.tree.Roots(), std::nullopt, 0);
            snapshot.documents.push_back(std::move(summary));
        }

        auto const* const active = impl_->Active();
        if (!active)
        {
            snapshot.document_title = "No document";
            snapshot.status_message = "Ready";
            return snapshot;
        }

        snapshot.has_document = true;
        snapshot.document_title = active->document.Title();
        auto const active_size = active->document.Size();
        snapshot.canvas_size = { .width = active_size.width, .height = active_size.height };
        snapshot.status_message = std::format("{} x {} pixels", active_size.width, active_size.height);
        snapshot.active_commands = snapshot.documents[
            static_cast<std::size_t>(active - impl_->documents.data())].commands;
        snapshot.active_layer_id = active->layers.active_layer_id;
        return snapshot;
    }
}
