#ifndef AUTO_UPGRADE_CORO_HPP
#define AUTO_UPGRADE_CORO_HPP

#include "command_func.h"
#include "network/network.h"
#include "vehicle_func.h"

#include <coroutine>

namespace AutoUpgradeRailType {

	inline std::coroutine_handle<> coro_resumer = nullptr;
	inline bool coro_waiting_for_callback = false;
	inline CommandCost coro_command_cost;
	inline VehicleID coro_new_vehicle_id;

	inline void ResetCoroState() {
		if (coro_resumer) {
			coro_resumer.destroy();
			coro_resumer = nullptr;
		}
		coro_waiting_for_callback = false;
	}

	inline bool HandleCoro() noexcept {
		if (coro_resumer) {
			if (coro_waiting_for_callback) {
				coro_waiting_for_callback = false;
				std::exchange(coro_resumer, nullptr)();
			}
			return true;
		}
		return false;
	}

	class DoCommandPAwaiter {
	private:
		TileIndex tile;
		uint32_t p1, p2, cmd;
		uint64_t p3;
		static void Callback(const CommandCost& result, TileIndex tile, uint32 p1, uint32 p2, uint64_t p3, uint32 cmd) {
			coro_command_cost = result;
			if (coro_command_cost.Failed()) {
				coro_command_cost = coro_command_cost;
			}
			coro_new_vehicle_id = _new_vehicle_id;
			coro_waiting_for_callback = true;
		}
	public:
		DoCommandPAwaiter(TileIndex tile, uint32_t p1, uint32_t p2, uint64_t p3, uint32_t cmd) :tile(tile), p1(p1), p2(p2), p3(p3), cmd(cmd) {}
		constexpr bool await_ready() const noexcept {
			return false;
		}
		static void CallbackWrapper(const CommandCost& result, TileIndex tile, uint32 p1, uint32 p2, uint64_t p3, uint32 cmd) {
			if (static_cast<uint16>(cmd) == CMD_FOUND_TOWN) {
				CcFoundRandomTown(result, tile, p1, p2, p3, cmd); // hack, since we can't introduce new callback indices, we repurpose an existing callback
				return;
			}
			Callback(result, tile, p1, p2, p3, cmd);
		}
		void await_suspend(std::coroutine_handle<> handle) {
			assert(!coro_resumer);
			coro_resumer = handle;
			if (!DoCommandPEx(tile, p1, p2, p3, cmd, CallbackWrapper, nullptr, 0) && _networking) {
				Callback(CommandCost(INVALID_STRING_ID), tile, p1, p2, p3, cmd);
			}
		}
		CommandCost await_resume() {
			return coro_command_cost;
		}
	};

	class WaitTickAwaiter {
	public:
		WaitTickAwaiter() {}
		constexpr bool await_ready() const noexcept {
			return false;
		}
		void await_suspend(std::coroutine_handle<> handle) {
			assert(!coro_resumer);
			coro_resumer = handle;
			coro_waiting_for_callback = true;
		}
		void await_resume() {
			return;
		}
	};

	inline DoCommandPAwaiter CoroDoCommandP(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd) {
		return DoCommandPAwaiter(tile, p1, p2, 0, cmd);
	}

	inline DoCommandPAwaiter CoroDoCommandPEx(TileIndex tile, uint32 p1, uint32 p2, uint64_t p3, uint32 cmd) {
		return DoCommandPAwaiter(tile, p1, p2, p3, cmd);
	}

	inline WaitTickAwaiter WaitTick() {
		return WaitTickAwaiter{};
	}

	struct Task {
		struct promise_type {
			Task get_return_object() {
				return Task{};
			}
			std::suspend_never initial_suspend() noexcept { return {}; }
			std::suspend_never final_suspend() noexcept { return {}; }
			void return_void() noexcept {}
			void unhandled_exception() noexcept {}
		};
	};

	struct AwaitableTask {
		struct promise_type {
			std::coroutine_handle<> resumer;
			AwaitableTask get_return_object() {
				return AwaitableTask{ *this };
			}
			std::suspend_always initial_suspend() noexcept { return {}; }
			std::suspend_never final_suspend() noexcept { resumer(); return {}; }
			void return_void() noexcept { }
			void unhandled_exception() noexcept {}
		};
		promise_type& promise;
		struct Awaiter {
			promise_type& promise;
			constexpr bool await_ready() const noexcept {
				return false;
			}
			void await_suspend(std::coroutine_handle<> handle) {
				promise.resumer = handle;
				std::coroutine_handle<promise_type>::from_promise(promise)();
			}
			void await_resume() {}
		};
		Awaiter operator co_await() {
			return Awaiter{ promise };
		}
	};

	inline AwaitableTask WaitTicks(size_t numticks) {
		while (--numticks > 0) {
			co_await WaitTick();
		}
	}

}

#endif // AUTO_UPGRADE_CORO_HPP
