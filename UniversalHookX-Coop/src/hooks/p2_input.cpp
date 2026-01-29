#include "p2_input.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>

#include "../backend.hpp"

#ifdef ENABLE_BACKEND_DX9
#include "backend/dx9/hook_manager/hook_manager.hpp"
#endif

namespace {
	// Мост ввода второго игрока: читаем IJKL/T на стороне Windows, логику применяет Ruby.
	constexpr uint32_t kUp = 1u << 0u;
	constexpr uint32_t kLeft = 1u << 1u;
	constexpr uint32_t kDown = 1u << 2u;
	constexpr uint32_t kRight = 1u << 3u;
	constexpr uint32_t kAttack = 1u << 4u;

	// Держим последний отправленный набор клавиш, чтобы не спамить RGSS одинаковыми данными.
	std::atomic<uint32_t> g_mask{0};
	uint32_t g_last_sent = 0xFFFFFFFFu;
	bool g_ruby_installed = false;

	uint32_t PollMask() {
		uint32_t mask = 0u;
		if ((GetAsyncKeyState('I') & 0x8000) != 0) {
			mask |= kUp;
		}
		if ((GetAsyncKeyState('J') & 0x8000) != 0) {
			mask |= kLeft;
		}
		if ((GetAsyncKeyState('K') & 0x8000) != 0) {
			mask |= kDown;
		}
		if ((GetAsyncKeyState('L') & 0x8000) != 0) {
			mask |= kRight;
		}
		if ((GetAsyncKeyState('T') & 0x8000) != 0) {
			mask |= kAttack;
		}
		return mask;
	}

	int MaskToDir(uint32_t mask) {
		// Преобразуем битмаску в направление RGSS (8/2/4/6).
		if (mask & kUp) {
			return 8;
		}
		if (mask & kDown) {
			return 2;
		}
		if (mask & kLeft) {
			return 4;
		}
		if (mask & kRight) {
			return 6;
		}
		return 0;
	}

#ifdef ENABLE_BACKEND_DX9
	void EnsureRubyHookInstalled() {
		if (g_ruby_installed || OriginalRGSSEval == nullptr) {
			return;
		}

		// Ruby-хелпер держит всю игровую логику на стороне движка (в его потоке).
		const char* ruby_code = R"uhx(
unless defined?($uhx_p2_installed)
  # Состояние, живущее между вызовами, для управления вторым игроком.
  $uhx_p2_installed = true
  $uhx_p2_dir = 0
  $uhx_p2_attack = false
  $uhx_p2_prev_attack = false

  module UHX_P2
    def self.set_state(dir, attack)
      # Принимаем состояние из C++.
      $uhx_p2_dir = dir.to_i
      $uhx_p2_attack = attack ? true : false
    end

    def self.frame_gate
      # Гейт по кадру: если Tick() вызывается несколько раз за кадр, отсекаем повторы.
      return true unless defined?(Graphics)
      fc = Graphics.frame_count
      return false if @last_frame == fc
      @last_frame = fc
      true
    end

    def self.active?
      # Работать только когда карта и игрок готовы (иначе в меню/паузе падает).
      return false unless defined?($game_map) && $game_map
      return false unless defined?($game_player) && $game_player
      true
    end

    def self.release_control
      # Вернуть то, что временно меняли у компаньона.
      if @companion && match_companion?(@companion)
        if @saved_move_type
          @companion.move_type = @saved_move_type if @companion.respond_to?(:move_type=)
          if @companion.respond_to?(:manual_move_type=)
            @companion.manual_move_type = @saved_manual_move_type
          end
        end
        npc = @companion.respond_to?(:npc) ? @companion.npc : nil
        if npc
          if @saved_no_aggro != nil && npc.respond_to?(:no_aggro=)
            npc.no_aggro = @saved_no_aggro
          end
          if @saved_ai_state != nil && npc.respond_to?(:ai_state=)
            npc.ai_state = @saved_ai_state
          end
        end
      end
      @saved_move_type = nil
      @saved_manual_move_type = nil
      @saved_no_aggro = nil
      @saved_ai_state = nil
      @companion = nil
    end

    def self.tick
      # Единственная точка обновления — вызывается из C++ каждый кадр.
      return unless frame_gate
      unless active?
        release_control
        return
      end
      update
    end

    def self.dir
      $uhx_p2_dir || 0
    end

    def self.attack_down?
      $uhx_p2_attack ? true : false
    end

    def self.attack_trigger?
      cur = $uhx_p2_attack ? true : false
      prev = $uhx_p2_prev_attack ? true : false
      $uhx_p2_prev_attack = cur
      cur && !prev
    end

    def self.match_companion?(ch)
      # Отфильтровываем "призраков": удалённые события и события с другой карты.
      return false unless ch && ch.respond_to?(:npc?) && ch.npc?
      return false if ch.respond_to?(:deleted?) && ch.deleted?
      if defined?($game_map) && $game_map && $game_map.respond_to?(:map_id)
        begin
          ch_map_id = ch.instance_variable_get(:@map_id)
          return false if ch_map_id && ch_map_id != $game_map.map_id
        rescue
        end
      end
      npc = ch.npc
      return false unless npc
      if defined?(Npc_CompanionOrkindSlayerCharge)
        return true if npc.is_a?(Npc_CompanionOrkindSlayerCharge)
      end
      name = npc.respond_to?(:npc_name) ? npc.npc_name.to_s : ""
      return true if name == "CompOrkindSlayer"
      return true if name.include?("CompOrkindSlayer")
      return true if name.include?("CompanionOrkindSlayer")
      false
    end

    def self.find_companion
      # Кешируем компаньона до смены карты, чтобы не сканировать всё каждый тик.
      if @companion && match_companion?(@companion)
        return @companion
      end
      @companion = nil
      @saved_move_type = nil
      @saved_manual_move_type = nil
      if $game_map
        if $game_map.respond_to?(:map_id)
          map_id = $game_map.map_id
          if @last_map_id != map_id
            @last_map_id = map_id
            @companion = nil
            @saved_move_type = nil
            @saved_manual_move_type = nil
          end
        end
        list = $game_map.instance_variable_get(:@all_characters) rescue nil
        if list
          list.each do |ch|
            next if ch.nil?
            if match_companion?(ch)
              @companion = ch
              break
            end
          end
        end
        if @companion.nil? && $game_map.respond_to?(:events)
          $game_map.events.each_value do |ev|
            if match_companion?(ev)
              @companion = ev
              break
            end
          end
        end
      end
      @companion
    end

    def self.pick_attack_skill(npc)
      # Берём реальный атакующий скилл из списков NPC.
      return nil if npc.nil?
      begin
        skill = npc.skills_killer.compact.first
        return skill if skill
        skill = npc.skills_assaulter.compact.first if npc.respond_to?(:skills_assaulter)
        return skill if skill
        skill = npc.skills_fucker.compact.first if npc.respond_to?(:skills_fucker)
        return skill if skill
      rescue
      end
      nil
    end

    def self.update
      comp = find_companion
      return unless comp
      dir = self.dir
      attack_trigger = attack_trigger?
      active = (dir && dir != 0) || attack_down?

      if comp.respond_to?(:move_type=)
        if active
          # Пока есть ввод игрока 2, переводим NPC в ручной режим.
          @saved_move_type = comp.move_type if @saved_move_type.nil?
          if comp.respond_to?(:get_manual_move_type)
            @saved_manual_move_type = comp.get_manual_move_type if @saved_manual_move_type.nil?
          end
          comp.manual_move_type = 0 if comp.respond_to?(:manual_move_type=)
          comp.move_type = 0
        elsif !@saved_move_type.nil?
          # Ввод пропал — возвращаем исходные настройки движения.
          comp.move_type = @saved_move_type
          if comp.respond_to?(:manual_move_type=)
            comp.manual_move_type = @saved_manual_move_type
          end
          @saved_move_type = nil
          @saved_manual_move_type = nil
        end
      end

      if comp.respond_to?(:through=)
        comp.through = false
      end
      if comp.respond_to?(:manual_priority=)
        comp.manual_priority = 1
      end
      if comp.respond_to?(:follower) && comp.follower.is_a?(Array)
        # Жёстко переводим в режим "ждать", чтобы не следовал за игроком.
        comp.follower[1] = 0
      end
      begin
        comp.instance_variable_set(:@manual_through, false)
      rescue
      end

      npc = comp.respond_to?(:npc) ? comp.npc : nil
      if npc
        if @saved_no_aggro.nil? && npc.respond_to?(:no_aggro)
          @saved_no_aggro = npc.no_aggro
        end
        if @saved_ai_state.nil? && npc.respond_to?(:ai_state)
          @saved_ai_state = npc.ai_state
        end
        # Отключаем AI/агро, чтобы поведение задавал только игрок 2.
        npc.no_aggro = true if npc.respond_to?(:no_aggro=)
        npc.ai_state = :none if npc.respond_to?(:ai_state=)
        begin
          npc.instance_variable_set(:@target, nil)
          npc.instance_variable_set(:@aggro_frame, 0)
          npc.instance_variable_set(:@alert_level, 0)
        rescue
        end
      end

      if attack_trigger
        if npc && (npc.action_state.nil? || npc.action_state == :none)
          skill = pick_attack_skill(npc)
          npc.launch_skill(skill, true) if skill
        end
      end

      # Блокируем движение, если игра сама его блокирует (стан/каст/анимации/захват).
      blocked = false
      blocked ||= comp.respond_to?(:moving?) && comp.moving?
      blocked ||= comp.respond_to?(:animation) && comp.animation
      blocked ||= comp.respond_to?(:skill_eff_reserved) && comp.skill_eff_reserved
      blocked ||= comp.respond_to?(:grabbed?) && comp.grabbed?
      blocked ||= npc && ![nil, :none].include?(npc.action_state)

      if dir && dir != 0 && !blocked
        begin
          # Логика задержки/поворота как в move_by_input из скриптов игры.
          current_dir = comp.direction if comp.respond_to?(:direction)
          count = comp.instance_variable_get(:@dirInputCount) || 0
          if current_dir && dir == current_dir && count == 0
            comp.direction = dir if comp.respond_to?(:direction=)
            comp.move_straight(dir)
          else
            count += 1
            comp.instance_variable_set(:@dirInputCount, count)
            comp.direction = dir if comp.respond_to?(:direction=)
            delay = System_Settings::GAME_DIR_INPUT_DELAY rescue 0
            comp.move_straight(dir) if count > delay
          end
        rescue
          comp.move_straight(dir)
        end
      else
        begin
          comp.instance_variable_set(:@dirInputCount, 0)
        rescue
        end
      end
    end
  end

end
)uhx";

		OriginalRGSSEval(ruby_code);
		g_ruby_installed = true;
	}
#endif
}

namespace P2Input {
	void Tick() {
#ifdef ENABLE_BACKEND_DX9
		if (OriginalRGSSEval == nullptr) {
			return;
		}

		EnsureRubyHookInstalled();

		uint32_t mask = PollMask();
		g_mask.store(mask, std::memory_order_relaxed);

		if (mask != g_last_sent) {
			int dir = MaskToDir(mask);
			bool attack = (mask & kAttack) != 0u;

			// Отправляем состояние в Ruby только при изменении.
			char code[128];
			std::snprintf(code, sizeof(code), "UHX_P2.set_state(%d,%s)", dir, attack ? "true" : "false");
			OriginalRGSSEval(code);

			g_last_sent = mask;
		}

		// Основной тик Ruby — вызывает поиск компаньона и применение ввода.
		OriginalRGSSEval("UHX_P2.tick if defined?(UHX_P2)");
#endif
	}
}
