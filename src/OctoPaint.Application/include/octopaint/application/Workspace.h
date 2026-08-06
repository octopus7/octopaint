#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace octopaint::application
{
    class PaintStrokeCommand;

    struct CanvasSize final
    {
        std::uint32_t width{};
        std::uint32_t height{};

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return width > 0 && height > 0;
        }

        auto operator<=>(CanvasSize const&) const = default;
    };

    // A process-unique, strongly typed document identity. A closed document's
    // identity is never reused, so stale UI state cannot target a new document.
    class DocumentId final
    {
    public:
        constexpr DocumentId() noexcept = default;

        [[nodiscard]] constexpr std::uint64_t Value() const noexcept
        {
            return value_;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value_ != 0;
        }

        auto operator<=>(DocumentId const&) const = default;

    private:
        explicit constexpr DocumentId(std::uint64_t const value) noexcept
            : value_(value)
        {
        }

        std::uint64_t value_{};

        friend class Workspace;
    };

    class LayerId final
    {
    public:
        constexpr LayerId() noexcept = default;

        [[nodiscard]] constexpr std::uint64_t Value() const noexcept
        {
            return value_;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value_ != 0;
        }

        auto operator<=>(LayerId const&) const = default;

    private:
        explicit constexpr LayerId(std::uint64_t const value) noexcept
            : value_(value)
        {
        }

        std::uint64_t value_{};

        friend class Workspace;
        friend class AddRasterLayerCommand;
        friend class AddGroupLayerCommand;
        friend class RemoveLayerCommand;
        friend class MoveLayerCommand;
    };

    enum class LayerKind : std::uint8_t
    {
        Raster,
        Group
    };

    enum class BlendMode : std::uint8_t
    {
        Normal,
        Multiply,
        Screen,
        Overlay,
        Darken,
        Lighten,
        ColorDodge,
        ColorBurn,
        SoftLight,
        HardLight,
        Difference,
        Exclusion,
        Hue,
        Saturation,
        Color,
        Luminosity
    };

    // The deliberately small mutation boundary exposed to application commands.
    // UI events should create a command and submit it to Workspace instead of
    // mutating documents directly.
    class DocumentMutation final
    {
    public:
        [[nodiscard]] std::string const& Title() const noexcept;
        [[nodiscard]] CanvasSize Size() const noexcept;
        void Rename(std::string title);

    private:
        explicit DocumentMutation(void* document, void* layers) noexcept;

        void* document_{};
        void* layers_{};

        friend class Workspace;
        friend class AddRasterLayerCommand;
        friend class AddGroupLayerCommand;
        friend class RemoveLayerCommand;
        friend class MoveLayerCommand;
        friend class RenameLayerCommand;
        friend class SetLayerOpacityCommand;
        friend class SetLayerVisibilityCommand;
        friend class SetLayerAlphaLockedCommand;
        friend class SetLayerBlendModeCommand;
        friend class PaintStrokeCommand;
    };

    class DocumentCommand
    {
    public:
        virtual ~DocumentCommand() = default;

        [[nodiscard]] virtual std::string Label() const = 0;
        virtual void Execute(DocumentMutation& document) = 0;
        virtual void Undo(DocumentMutation& document) = 0;
    };

    class RenameDocumentCommand final : public DocumentCommand
    {
    public:
        explicit RenameDocumentCommand(std::string new_title);

        [[nodiscard]] std::string Label() const override;
        void Execute(DocumentMutation& document) override;
        void Undo(DocumentMutation& document) override;

    private:
        std::string new_title_;
        std::string previous_title_;
        bool captured_previous_title_{};
    };

    class AddRasterLayerCommand final : public DocumentCommand
    {
    public:
        explicit AddRasterLayerCommand(
            std::string name,
            std::optional<LayerId> parent = std::nullopt,
            std::optional<std::size_t> index = std::nullopt);

        [[nodiscard]] LayerId CreatedLayerId() const noexcept;
        [[nodiscard]] std::string Label() const override;
        void Execute(DocumentMutation& document) override;
        void Undo(DocumentMutation& document) override;

    private:
        LayerId id_;
        std::string name_;
        std::optional<LayerId> parent_;
        std::optional<std::size_t> requested_index_;
        std::size_t insertion_index_{};
        std::optional<LayerId> previous_active_layer_;
        bool executed_once_{};
    };

    class AddGroupLayerCommand final : public DocumentCommand
    {
    public:
        explicit AddGroupLayerCommand(
            std::string name,
            std::optional<LayerId> parent = std::nullopt,
            std::optional<std::size_t> index = std::nullopt);

        [[nodiscard]] LayerId CreatedLayerId() const noexcept;
        [[nodiscard]] std::string Label() const override;
        void Execute(DocumentMutation& document) override;
        void Undo(DocumentMutation& document) override;

    private:
        LayerId id_;
        std::string name_;
        std::optional<LayerId> parent_;
        std::optional<std::size_t> requested_index_;
        std::size_t insertion_index_{};
        std::optional<LayerId> previous_active_layer_;
        bool executed_once_{};
    };

    class RemoveLayerCommand final : public DocumentCommand
    {
    public:
        explicit RemoveLayerCommand(LayerId id);
        ~RemoveLayerCommand() override;

        [[nodiscard]] std::string Label() const override;
        void Execute(DocumentMutation& document) override;
        void Undo(DocumentMutation& document) override;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

    class MoveLayerCommand final : public DocumentCommand
    {
    public:
        MoveLayerCommand(
            LayerId id,
            std::optional<LayerId> new_parent,
            std::size_t new_index);

        [[nodiscard]] std::string Label() const override;
        void Execute(DocumentMutation& document) override;
        void Undo(DocumentMutation& document) override;

    private:
        LayerId id_;
        std::optional<LayerId> new_parent_;
        std::size_t new_index_{};
        std::optional<LayerId> previous_parent_;
        std::size_t previous_index_{};
        bool captured_previous_location_{};
    };

    class RenameLayerCommand final : public DocumentCommand
    {
    public:
        RenameLayerCommand(LayerId id, std::string new_name);

        [[nodiscard]] std::string Label() const override;
        void Execute(DocumentMutation& document) override;
        void Undo(DocumentMutation& document) override;

    private:
        LayerId id_;
        std::string new_name_;
        std::string previous_name_;
        bool captured_previous_name_{};
    };

    class SetLayerOpacityCommand final : public DocumentCommand
    {
    public:
        SetLayerOpacityCommand(LayerId id, float opacity);

        [[nodiscard]] std::string Label() const override;
        void Execute(DocumentMutation& document) override;
        void Undo(DocumentMutation& document) override;

    private:
        LayerId id_;
        float opacity_{};
        float previous_opacity_{};
        bool captured_previous_opacity_{};
    };

    class SetLayerVisibilityCommand final : public DocumentCommand
    {
    public:
        SetLayerVisibilityCommand(LayerId id, bool visible);

        [[nodiscard]] std::string Label() const override;
        void Execute(DocumentMutation& document) override;
        void Undo(DocumentMutation& document) override;

    private:
        LayerId id_;
        bool visible_{};
        bool previous_visibility_{};
        bool captured_previous_visibility_{};
    };

    class SetLayerAlphaLockedCommand final : public DocumentCommand
    {
    public:
        SetLayerAlphaLockedCommand(LayerId id, bool alpha_locked);

        [[nodiscard]] std::string Label() const override;
        void Execute(DocumentMutation& document) override;
        void Undo(DocumentMutation& document) override;

    private:
        LayerId id_;
        bool alpha_locked_{};
        bool previous_alpha_locked_{};
        bool captured_previous_alpha_locked_{};
    };

    class SetLayerBlendModeCommand final : public DocumentCommand
    {
    public:
        SetLayerBlendModeCommand(LayerId id, BlendMode blend_mode);

        [[nodiscard]] std::string Label() const override;
        void Execute(DocumentMutation& document) override;
        void Undo(DocumentMutation& document) override;

    private:
        LayerId id_;
        BlendMode blend_mode_{ BlendMode::Normal };
        BlendMode previous_blend_mode_{ BlendMode::Normal };
        bool captured_previous_blend_mode_{};
    };

    struct CommandAvailability final
    {
        bool can_undo{};
        bool can_redo{};
        std::string undo_label;
        std::string redo_label;
    };

    // A detached, flat pre-order representation. parent_id, depth and
    // sibling_index let any frontend reconstruct the tree without Core types.
    struct LayerSummary final
    {
        LayerId id;
        std::optional<LayerId> parent_id;
        std::size_t sibling_index{};
        std::size_t depth{};
        LayerKind kind{ LayerKind::Raster };
        std::string name;
        bool visible{ true };
        bool locked{};
        bool alpha_locked{};
        float opacity{ 1.0F };
        BlendMode blend_mode{ BlendMode::Normal };
    };

    struct DocumentSummary final
    {
        DocumentId id;
        std::string title;
        CanvasSize canvas_size;
        bool is_active{};
        bool is_dirty{};
        std::uint64_t current_revision{};
        std::uint64_t saved_revision{};
        CommandAvailability commands;
        std::vector<LayerSummary> layers;
        std::optional<LayerId> active_layer_id;
    };

    // This is a detached value snapshot: modifying the returned values never
    // mutates Workspace. The four leading fields preserve the original API.
    struct WorkspaceSnapshot final
    {
        bool has_document{};
        std::string document_title;
        CanvasSize canvas_size;
        std::string status_message;

        std::vector<DocumentSummary> documents;
        std::optional<DocumentId> active_document_id;
        CommandAvailability active_commands;
        std::optional<LayerId> active_layer_id;
    };

    enum class PaintTool : std::uint8_t
    {
        Pencil,
        Airbrush
    };

    struct PaintColorRgba8 final
    {
        std::uint8_t red{};
        std::uint8_t green{};
        std::uint8_t blue{};
        std::uint8_t alpha{ 255 };

        auto operator<=>(PaintColorRgba8 const&) const = default;
    };

    struct PaintBrushOptions final
    {
        float radius{ 8.0F };
        float flow_per_second{ 1.0F };
        float fixed_timestep_seconds{ 1.0F / 60.0F };
        float hardness{ 1.0F };
        float spacing{ 0.25F };
        float opacity{ 1.0F };
        bool pressure_affects_size{};
        bool pressure_affects_opacity{ true };
    };

    struct PaintPressureOptions final
    {
        float minimum_input{};
        float maximum_input{ 1.0F };
        float gamma{ 1.0F };
    };

    struct PaintStabilizerOptions final
    {
        bool enabled{};
        float strength{ 0.5F };
    };

    struct PaintPointerSample final
    {
        double x{};
        double y{};
        float pressure{ 1.0F };
        // Time since the preceding sample. The first sample may use zero.
        double elapsed_seconds{};
        // Starts a disconnected stroke segment. Stabilizer and dab spacing
        // state reset without creating a second history entry.
        bool begins_new_segment{};
    };

    struct PaintStrokeRequest final
    {
        DocumentId document_id;
        LayerId layer_id;
        PaintTool tool{ PaintTool::Pencil };
        PaintColorRgba8 color;
        PaintBrushOptions brush;
        PaintPressureOptions pressure;
        PaintStabilizerOptions stabilizer;
        std::vector<PaintPointerSample> samples;
    };

    enum class PaintStrokeStatus : std::uint8_t
    {
        Applied,
        DocumentNotFound,
        LayerNotFound,
        LayerNotRaster,
        LayerLocked,
        InvalidRequest,
        NoPixels
    };

    struct PixelBounds final
    {
        std::int32_t x{};
        std::int32_t y{};
        std::uint32_t width{};
        std::uint32_t height{};

        auto operator<=>(PixelBounds const&) const = default;
    };

    struct PaintStrokeResult final
    {
        PaintStrokeStatus status{ PaintStrokeStatus::InvalidRequest };
        std::optional<PixelBounds> changed_bounds;

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return status == PaintStrokeStatus::Applied;
        }
    };

    // A detached, tightly packed canvas-sized BGRA8 premultiplied snapshot of
    // one raster layer. The revision lets a renderer reject stale frames.
    struct RasterPixelSnapshot final
    {
        DocumentId document_id;
        LayerId layer_id;
        CanvasSize size;
        std::uint64_t revision{};
        std::size_t row_stride{};
        std::vector<std::byte> pixels_bgra_premultiplied;
    };

    class Workspace final
    {
    public:
        Workspace();
        ~Workspace();

        Workspace(Workspace&&) noexcept;
        Workspace& operator=(Workspace&&) noexcept;

        Workspace(Workspace const&) = delete;
        Workspace& operator=(Workspace const&) = delete;

        // Source-compatible migration: existing callers may ignore the new ID.
        DocumentId NewDocument(std::string title, CanvasSize size);
        [[nodiscard]] bool CloseDocument(DocumentId id);
        [[nodiscard]] bool ActivateDocument(DocumentId id);
        [[nodiscard]] std::optional<DocumentId> ActiveDocument() const noexcept;
        [[nodiscard]] bool Contains(DocumentId id) const noexcept;
        [[nodiscard]] bool ActivateLayer(DocumentId document_id, LayerId layer_id);
        [[nodiscard]] bool ActivateLayer(LayerId layer_id);

        void ExecuteCommand(DocumentId id, std::unique_ptr<DocumentCommand> command);
        [[nodiscard]] bool Undo(DocumentId id);
        [[nodiscard]] bool Redo(DocumentId id);
        void MarkSaved(DocumentId id);

        void ExecuteCommand(std::unique_ptr<DocumentCommand> command);
        [[nodiscard]] bool Undo();
        [[nodiscard]] bool Redo();
        void MarkActiveDocumentSaved();

        // Applies all samples as one document-history entry. A non-Applied
        // result leaves both pixels and history unchanged.
        [[nodiscard]] PaintStrokeResult ApplyPaintStroke(PaintStrokeRequest const& request);
        [[nodiscard]] std::optional<RasterPixelSnapshot> SnapshotRasterLayerPixels(
            DocumentId document_id,
            LayerId layer_id) const;

        [[nodiscard]] WorkspaceSnapshot Snapshot() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
