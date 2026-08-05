#include <octopaint/core/Layer.h>
#include <octopaint/core/Tile.h>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using namespace octopaint::core;

    void Require(bool const condition, char const* const message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    template<typename Function>
    void RequireInvalidArgument(Function&& function, char const* const message)
    {
        try
        {
            std::forward<Function>(function)();
        }
        catch (std::invalid_argument const&)
        {
            return;
        }

        throw std::runtime_error(message);
    }

    [[nodiscard]] LayerProperties Properties(
        std::uint64_t const id,
        std::string name,
        float const opacity = 1.0F,
        BlendMode const blend_mode = BlendMode::Normal)
    {
        return {
            .id = LayerId(id),
            .name_utf8 = std::move(name),
            .visible = true,
            .locked = false,
            .opacity = opacity,
            .blend_mode = blend_mode
        };
    }

    void TestSparseTileStore()
    {
        constexpr TileKey first_key{ .x = -2, .y = 3, .level = 0 };
        constexpr TileKey second_key{ .x = 5, .y = -1, .level = 0 };

        SparseTileStore store;
        Require(store.Empty(), "A new sparse tile store must be empty.");
        Require(!store.Read(first_key).has_value(), "Reading an absent tile must not allocate it.");

        auto first_draft = store.BeginWrite(first_key);
        Require(first_draft.Pixels().size() == Rgba8TileByteCount, "A write draft must cover exactly one 256 x 256 RGBA8 tile.");
        first_draft.Pixels()[0] = std::byte{ 0x80 };
        first_draft.Pixels()[3] = std::byte{ 0x80 };
        Require(store.Publish(first_key, std::move(first_draft)) == TilePublishResult::Stored, "A visible tile must be stored.");

        auto second_draft = store.BeginWrite(second_key);
        second_draft.Pixels()[4] = std::byte{ 0x40 };
        second_draft.Pixels()[7] = std::byte{ 0x40 };
        Require(store.Publish(second_key, std::move(second_draft)) == TilePublishResult::Stored, "A second visible tile must be stored.");
        Require(store.TileCount() == 2, "Only published visible tiles may occupy the sparse store.");

        SparseTileStore snapshot = store;
        auto const original_first = store.Read(first_key);
        auto const snapshot_first = snapshot.Read(first_key);
        auto const original_second = store.Read(second_key);
        auto const snapshot_second = snapshot.Read(second_key);
        Require(original_first && snapshot_first && original_first->SharesStorageWith(*snapshot_first),
            "Copying a tile store must share immutable tile payloads.");
        Require(original_second && snapshot_second && original_second->SharesStorageWith(*snapshot_second),
            "Every untouched payload must remain shared after a store copy.");

        auto changed = snapshot.BeginWrite(first_key);
        changed.Pixels()[0] = std::byte{ 0x20 };
        changed.Pixels()[3] = std::byte{ 0x20 };
        Require(snapshot.Publish(first_key, std::move(changed)) == TilePublishResult::Stored, "Publishing a changed draft must replace that tile.");

        auto const changed_first = snapshot.Read(first_key);
        Require(changed_first && !original_first->SharesStorageWith(*changed_first),
            "Writing one tile must detach only that payload from the shared snapshot.");
        Require(original_first->Pixels()[0] == std::byte{ 0x80 }, "COW updates must leave the original snapshot unchanged.");
        Require(changed_first->Pixels()[0] == std::byte{ 0x20 }, "COW updates must publish the edited bytes.");
        Require(original_second->SharesStorageWith(*snapshot.Read(second_key)), "COW updates must keep unrelated tiles shared.");

        auto transparent = snapshot.BeginWrite(first_key);
        for (std::size_t index = 3; index < transparent.Pixels().size(); index += Rgba8BytesPerPixel)
        {
            transparent.Pixels()[index] = std::byte{ 0 };
        }
        Require(snapshot.Publish(first_key, std::move(transparent)) == TilePublishResult::RemovedTransparent,
            "Publishing a fully transparent tile must remove its sparse allocation.");
        Require(!snapshot.Read(first_key), "A removed transparent tile must read as absent.");

        auto const keys = store.Keys();
        Require(keys.size() == 2 && keys[0] == first_key && keys[1] == second_key,
            "Sparse tile iteration keys must have deterministic TileKey order.");

        std::vector<std::byte> invalid_bytes(17);
        RequireInvalidArgument(
            [&] { static_cast<void>(TilePayload::FromRgba8(invalid_bytes)); },
            "Tile payload construction must reject non-tile byte counts.");
    }

    void TestLayerProperties()
    {
        Require(!LayerId{}.IsValid(), "The default layer ID must be invalid.");
        Require(LayerId(9).IsValid() && LayerId(9).Value() == 9, "A non-zero layer ID must preserve its stable value.");
        Require(BlendMode::Normal != BlendMode::Multiply, "Blend mode values must be distinct contracts.");

        auto raster = RasterLayer(Properties(11, "Paint", 0.75F, BlendMode::Multiply));
        Require(raster.Kind() == LayerKind::Raster, "RasterLayer must report its domain kind.");
        Require(raster.Properties().opacity == 0.75F, "Layer opacity must be preserved.");
        Require(raster.Properties().blend_mode == BlendMode::Multiply, "Layer blend mode must be preserved.");
        Require(!raster.Properties().alpha_locked, "Layer alpha lock must default to disabled.");

        raster.Rename("Ink");
        raster.SetVisible(false);
        raster.SetLocked(true);
        raster.SetAlphaLocked(true);
        raster.SetOpacity(0.25F);
        raster.SetBlendMode(BlendMode::Screen);
        Require(raster.Properties().name_utf8 == "Ink" && !raster.Properties().visible
            && raster.Properties().locked && raster.Properties().alpha_locked,
            "Common layer properties must be editable on every concrete layer.");
        Require(raster.Properties().opacity == 0.25F && raster.Properties().blend_mode == BlendMode::Screen,
            "Common opacity and blend mode updates must be retained.");

        RequireInvalidArgument([&] { raster.Rename(""); }, "An empty layer rename must fail deterministically.");
        RequireInvalidArgument([&] { raster.SetOpacity(std::nanf("")); }, "A non-finite opacity must fail deterministically.");
        RequireInvalidArgument([&] { raster.SetOpacity(1.01F); }, "An opacity above one must fail deterministically.");
        RequireInvalidArgument(
            [] { static_cast<void>(RasterLayer(Properties(0, "Invalid"))); },
            "A zero layer ID must be rejected at construction.");
    }

    void TestOrderedNestedLayerTree()
    {
        LayerTree tree;
        Require(tree.AppendRoot(std::make_unique<RasterLayer>(Properties(2, "Background"))).Succeeded(), "A root raster layer must be accepted.");
        Require(tree.AppendRoot(std::make_unique<GroupLayer>(Properties(1, "Characters"))).Succeeded(), "A root group layer must be accepted.");
        Require(tree.AppendChild(LayerId(1), std::make_unique<RasterLayer>(Properties(3, "Line art"))).Succeeded(),
            "A raster layer must be accepted below a group.");
        Require(tree.AppendChild(LayerId(1), std::make_unique<GroupLayer>(Properties(4, "Color"))).Succeeded(),
            "Nested groups must be supported.");
        Require(tree.AppendChild(LayerId(4), std::make_unique<RasterLayer>(Properties(5, "Base color"))).Succeeded(),
            "Arbitrary group nesting must preserve child ownership.");

        Require(tree.Roots().size() == 2 && tree.Roots()[0]->Properties().id == LayerId(2)
            && tree.Roots()[1]->Properties().id == LayerId(1),
            "Root insertion order must be preserved.");
        auto const* group = dynamic_cast<GroupLayer const*>(tree.Find(LayerId(1)));
        Require(group != nullptr && group->Children().size() == 2, "Stable IDs must locate nested-capable group layers.");
        Require(group->Children()[0]->Properties().id == LayerId(3) && group->Children()[1]->Properties().id == LayerId(4),
            "Child insertion order must be preserved.");

        auto const duplicate = tree.AppendRoot(std::make_unique<RasterLayer>(Properties(3, "Duplicate")));
        Require(!duplicate && duplicate.code == LayerValidationCode::DuplicateId,
            "Duplicate IDs must be rejected with a deterministic validation code.");
        auto const raster_parent = tree.AppendChild(LayerId(2), std::make_unique<RasterLayer>(Properties(6, "Invalid child")));
        Require(!raster_parent && raster_parent.code == LayerValidationCode::ParentIsNotGroup,
            "Raster layers must reject children with a deterministic validation code.");
        auto const absent_parent = tree.AppendChild(LayerId(99), std::make_unique<RasterLayer>(Properties(6, "Orphan")));
        Require(!absent_parent && absent_parent.code == LayerValidationCode::ParentNotFound,
            "Missing parents must be distinguished from non-group parents.");

        Require(tree.Move(LayerId(2), LayerId(1), 1).Succeeded(), "A root layer must be movable into a group at a final ordered index.");
        group = dynamic_cast<GroupLayer const*>(tree.Find(LayerId(1)));
        Require(group->Children().size() == 3 && group->Children()[0]->Properties().id == LayerId(3)
            && group->Children()[1]->Properties().id == LayerId(2) && group->Children()[2]->Properties().id == LayerId(4),
            "Move must use final destination order semantics.");

        auto const cycle = tree.Move(LayerId(1), LayerId(4), 0);
        Require(!cycle && cycle.code == LayerValidationCode::CannotMoveIntoDescendant,
            "Moving a group into its descendant must be rejected before mutation.");
        auto const self_parent = tree.Move(LayerId(4), LayerId(4), 0);
        Require(!self_parent && self_parent.code == LayerValidationCode::CannotParentToSelf,
            "Moving a layer into itself must have a distinct deterministic error.");

        Require(tree.Move(LayerId(4), std::nullopt, 0).Succeeded(), "A nested group must be movable back to the root.");
        Require(tree.Roots().size() == 2 && tree.Roots()[0]->Properties().id == LayerId(4)
            && tree.Roots()[1]->Properties().id == LayerId(1),
            "Cross-parent moves must preserve the requested final ordering.");
        Require(tree.Move(LayerId(1), std::nullopt, 0).Succeeded(), "A same-parent reorder must succeed.");
        Require(tree.Roots()[0]->Properties().id == LayerId(1) && tree.Roots()[1]->Properties().id == LayerId(4),
            "Same-parent moves must interpret the index after removal.");

        auto removed = tree.Remove(LayerId(3));
        Require(removed && removed.layer && removed.layer->Properties().id == LayerId(3),
            "Remove must return the owned layer without changing its stable ID.");
        Require(tree.Find(LayerId(3)) == nullptr, "A removed layer must no longer be discoverable.");
        auto missing = tree.Remove(LayerId(77));
        Require(!missing && missing.result.code == LayerValidationCode::LayerNotFound,
            "Removing a missing layer must return a deterministic error.");
        Require(tree.Validate().empty(), "A tree produced by validated operations must pass deterministic pre-order validation.");
    }
}

int main()
{
    try
    {
        TestSparseTileStore();
        TestLayerProperties();
        TestOrderedNestedLayerTree();
        std::cout << "OctoPaint Core domain tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (std::exception const& error)
    {
        std::cerr << "OctoPaint Core domain tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
