#pragma once



#include "EngineInitData.hpp"
#include "Time.hpp"
#include "Systems/GpuResouceManager.hpp"
#include "Systems/Renderer.hpp"
#include "Systems/InputSystem.hpp"
#include "Systems/EntityConnector.hpp"
#include "Entities/EntityType.hpp"
#include "Entities/EntityHandle.hpp"
#include "Entities/Entity.hpp"
#include "GeometryPrimitives/Circle.hpp"
#include "Entities/EntitySpawnerFromPools.hpp"
#include "Components/AnimationMetaData.hpp"
#include "Systems/Grid.hpp"
#include "Systems/MemoryAlloc.hpp"
#include "Systems/CallbacksTimer.hpp"
#include "Systems/TimeRewind/TimeRewind.hpp"
#include <vector>
#include <glm.hpp>
#include "Systems/EventSystem/EventManager.hpp"


struct SDL_Window;
struct SDL_Renderer;

namespace Asteroid
{


	class Engine
	{

	public:

		explicit Engine(EngineInitData&& l_initialData);

		bool Init();


		bool GameLoop();


		Entity& GetEntityFromHandle(const EntityHandle l_entityHandle);
		const Entity& GetEntityFromHandle(const EntityHandle l_entityHandle) const;

		//Iterates entities and returns the first one that has that type
		Entity& GetEntityFromType(const EntityType l_type);
		const Entity& GetEntityFromType(const EntityType l_type) const;


		const std::vector<Entity>& GetEntities() const;



		const AnimationMetaData* GetAnimationMeta(const AnimationType l_type) const;

		RenderSystem::Renderer* GetRenderer();

		const InputSystem& GetInputSystem() const;

		void GetCurrentWindowSize(glm::ivec2& l_windowRes) const;

		const Grid& GetGrid() const;

		const std::vector<Circle>& GetCircleBounds() const;
		void UpdateCircleBounds();

		CallbacksTimer& GetCallbacksTimer();

		~Engine();

	public:

		EntityConnector m_entityConnector;

	private:

		void InitEntitiesAndPools();

	private:

		EngineInitData m_initialData;

		//Wanted to have pointer here but vector of entities might grow
		//and invalidate the ptr.
		uint32_t m_playerEntityHandle{};
		std::vector<Entity> m_entities{};
		std::vector<Circle> m_circleBoundsEntities{};
		std::vector<AnimationMetaData> m_animationMetaData;
		Time m_trackLastFrameElapsedTime;
		float m_timeSinceStartInSeconds{};
		uint32_t m_currentLevel{1U};

		uint32_t m_backgroundStarsTextureHandle{};
		uint32_t m_backgroundStars2TextureHandle{};

		RenderSystem::Renderer m_renderer;
		InputSystem m_inputSystem;
		EntitySpawnerFromPools m_entitySpawnerFromPools;
		Grid m_grid;
		CallbacksTimer m_callbacksTimer{};
		TimeRewind m_timeRewind{};

		MemoryAlloc m_allocator{};

		EventManager m_eventManager{};

		GpuResourceManager m_gpuResourceManager;

		SDL_Window* m_window;
	};


}