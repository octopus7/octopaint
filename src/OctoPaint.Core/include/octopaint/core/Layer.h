#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <octopaint/core/Tile.h>

namespace octopaint::core
{
    class LayerId final
    {
    public:
        constexpr LayerId() noexcept = default;
        explicit constexpr LayerId(std::uint64_t const value) noexcept : value_(value) {}

        [[nodiscard]] constexpr std::uint64_t Value() const noexcept { return value_; }
        [[nodiscard]] constexpr bool IsValid() const noexcept { return value_ != 0; }

        auto operator<=>(LayerId const&) const = default;

    private:
        std::uint64_t value_{};
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

    enum class LayerKind : std::uint8_t
    {
        Raster,
        Group
    };

    struct LayerProperties final
    {
        LayerId id;
        std::string name_utf8;
        bool visible{ true };
        bool locked{ false };
        bool alpha_locked{ false };
        float opacity{ 1.0F };
        BlendMode blend_mode{ BlendMode::Normal };
    };

    enum class LayerValidationCode : std::uint8_t
    {
        None,
        NullLayer,
        InvalidId,
        DuplicateId,
        EmptyName,
        InvalidOpacity,
        ParentNotFound,
        ParentIsNotGroup,
        LayerNotFound,
        IndexOutOfRange,
        CannotParentToSelf,
        CannotMoveIntoDescendant
    };

    [[nodiscard]] std::string_view LayerValidationMessage(LayerValidationCode code) noexcept;
    [[nodiscard]] LayerValidationCode ValidateLayerProperties(LayerProperties const& properties) noexcept;

    struct LayerMutationResult final
    {
        LayerValidationCode code{ LayerValidationCode::None };
        LayerId subject;

        [[nodiscard]] constexpr bool Succeeded() const noexcept
        {
            return code == LayerValidationCode::None;
        }

        explicit constexpr operator bool() const noexcept { return Succeeded(); }
    };

    struct LayerValidationIssue final
    {
        LayerValidationCode code{ LayerValidationCode::None };
        LayerId subject;

        auto operator<=>(LayerValidationIssue const&) const = default;
    };

    class Layer
    {
    public:
        virtual ~Layer() = default;

        Layer(Layer const&) = delete;
        Layer& operator=(Layer const&) = delete;
        Layer(Layer&&) noexcept = default;
        Layer& operator=(Layer&&) noexcept = default;

        [[nodiscard]] virtual LayerKind Kind() const noexcept = 0;
        [[nodiscard]] LayerProperties const& Properties() const noexcept;

        void Rename(std::string name_utf8);
        void SetVisible(bool visible) noexcept;
        void SetLocked(bool locked) noexcept;
        void SetAlphaLocked(bool alpha_locked) noexcept;
        void SetOpacity(float opacity);
        void SetBlendMode(BlendMode blend_mode) noexcept;

    protected:
        explicit Layer(LayerProperties properties);

    private:
        LayerProperties properties_;
    };

    class RasterLayer final : public Layer
    {
    public:
        explicit RasterLayer(LayerProperties properties);
        RasterLayer(LayerProperties properties, SparseTileStore tiles);

        [[nodiscard]] LayerKind Kind() const noexcept override;
        [[nodiscard]] SparseTileStore const& Tiles() const noexcept;
        [[nodiscard]] SparseTileStore& Tiles() noexcept;

    private:
        SparseTileStore tiles_;
    };

    class GroupLayer final : public Layer
    {
    public:
        explicit GroupLayer(LayerProperties properties);

        [[nodiscard]] LayerKind Kind() const noexcept override;
        [[nodiscard]] std::span<std::unique_ptr<Layer> const> Children() const noexcept;

    private:
        friend class LayerTree;
        friend struct LayerTreeAccess;
        std::vector<std::unique_ptr<Layer>> children_;
    };

    struct LayerRemovalResult final
    {
        LayerMutationResult result;
        std::unique_ptr<Layer> layer;

        [[nodiscard]] explicit operator bool() const noexcept { return result.Succeeded(); }
    };

    class LayerTree final
    {
    public:
        [[nodiscard]] std::span<std::unique_ptr<Layer> const> Roots() const noexcept;
        [[nodiscard]] Layer* Find(LayerId id) noexcept;
        [[nodiscard]] Layer const* Find(LayerId id) const noexcept;

        [[nodiscard]] LayerMutationResult AppendRoot(std::unique_ptr<Layer> layer);
        [[nodiscard]] LayerMutationResult AppendChild(LayerId parent, std::unique_ptr<Layer> layer);
        [[nodiscard]] LayerMutationResult Insert(
            std::optional<LayerId> parent,
            std::size_t index,
            std::unique_ptr<Layer> layer);
        [[nodiscard]] LayerRemovalResult Remove(LayerId id);
        [[nodiscard]] LayerMutationResult Move(
            LayerId id,
            std::optional<LayerId> new_parent,
            std::size_t final_index);

        [[nodiscard]] std::vector<LayerValidationIssue> Validate() const;

    private:
        std::vector<std::unique_ptr<Layer>> roots_;
    };
}
