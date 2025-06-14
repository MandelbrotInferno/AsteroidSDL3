

#include "Engine.hpp"
#include "Components/MovementComponents/UserInputBasedMovementComponent.hpp"
#include "Components/OnceRepeatableAnimationComponent.hpp"
#include "Components/IndefiniteRepeatableAnimationComponent.hpp"
#include "Components/RayMovementComponent.hpp"
#include "Components/StateComponents/ActiveBasedStateComponent.hpp"
#include "Components/CollisionComponents/PlayerCollisionComponent.hpp"
#include "Components/CollisionComponents/AsteroidCollisionComponent.hpp"
#include "Components/CollisionComponents/BulletCollisionComponent.hpp"
#include "Components/AttributeComponents/AsteroidAttributeComponent.hpp"
#include "Components/AttributeComponents/PlayerAttributeComponent.hpp"
#include "Components/AttributeComponents/CursorAttributeComponent.hpp"
#include "Systems/Colors.hpp"
#include "Systems/RenderingData.hpp"
#include "Components/UpdateComponents.hpp"
#include "Systems/LogSystem.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_time.h>
#include <memory>

namespace Asteroid
{
	Engine::Engine(EngineInitData&& l_initialData)
		:m_initialData(std::move(l_initialData))
		,m_window(nullptr)
		,m_renderer(&m_gpuResourceManager)
		,m_inputSystem()
		,m_entityConnector(this)
		,m_entitySpawnerFromPools(this)
		,m_grid()
		,m_animationMetaData(std::move(m_initialData.m_animationMetaData))
	{
		
	}


	bool Engine::Init()
	{
		using namespace LogSystem;
		LOG(Severity::INFO, Channel::INITIALIZATION, "****Engine initialization has begun****\n");

		if (false == SDL_SetAppMetadata(m_initialData.m_appName.c_str()
			, m_initialData.m_appVersion.c_str(), nullptr)) {

			LOG(Severity::FAILURE, Channel::INITIALIZATION, "SDL Failed to create metadata for the app.");

		}



		Set_Verbosity(Severity::FAILURE);

		LOG(Severity::INFO, Channel::INITIALIZATION, "Metadata creation was successfull.");

		if (false == SDL_InitSubSystem(SDL_INIT_VIDEO)) {

			LOG(Severity::FAILURE, Channel::INITIALIZATION
				, "SDL failed to initialize at least one of the requested subsystems: %s", SDL_GetError());
			return false;
		}

		LOG(Severity::INFO, Channel::INITIALIZATION
			, "Initialization of all the requested subsystems was successfull.");

		
		m_window = SDL_CreateWindow
			(m_initialData.m_windowTitle.c_str(),0,0
			, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
		
		if(nullptr == m_window){

			LOG(Severity::FAILURE, Channel::INITIALIZATION, "SDL failed to create window: %s", SDL_GetError());
			return false;

		}

		LOG(Severity::INFO, Channel::INITIALIZATION, "Window creation was successfull.");

		
		m_renderer.Init(m_window);
		m_renderer.SetClearColor(RenderSystem::Colors::BLACK);

		LOG(Severity::INFO, Channel::INITIALIZATION, "Creating textures on Gpu commencing....");

		for (auto& l_mapPairNameToPath : m_initialData.m_mappedTextureNamesToTheirPaths) {
			
			m_gpuResourceManager.CreateGpuTextureReturnHandle(m_renderer.GetSDLRenderer()
				, l_mapPairNameToPath.second, l_mapPairNameToPath.first);
		}

		m_backgroundStarsTextureHandle = m_gpuResourceManager.RetrieveGpuTextureHandle("BackgroundStarClusters");
		m_backgroundStars2TextureHandle = m_gpuResourceManager.RetrieveGpuTextureHandle("BackgroundStarClusters2");

		LOG(Severity::INFO, Channel::INITIALIZATION, "Creation of all textures on gpu was successful.");


		LOG(Severity::INFO, Channel::INITIALIZATION, "Initializing the entities....");

		Set_Verbosity(Severity::WARNING);

		InitEntitiesAndPools();

		glm::ivec2 lv_fullWindowSize{};
		GetCurrentWindowSize(lv_fullWindowSize);
		m_grid.Init(lv_fullWindowSize);

		LOG(Severity::INFO, Channel::INITIALIZATION, "Initializing entities was successful.");




		return true;
	}


	bool Engine::GameLoop()
	{
		using namespace LogSystem;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();
		ImGui_ImplSDL3_InitForSDLRenderer(m_window, m_renderer.GetSDLRenderer());
		ImGui_ImplSDLRenderer3_Init(m_renderer.GetSDLRenderer());


		bool lv_quit = false;
		bool show_demo_window = true;
		bool show_another_window = false;
		ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 1.00f);
		constexpr float lv_60fpsInMilliseconds{16.6666667f};
		UpdateComponents lv_updateComponent{};
		lv_updateComponent.m_engine = this;

		glm::ivec2 lv_currentWindowSize{};
		RenderSystem::RenderingData lv_backgroundStarsRenderData{};
		lv_backgroundStarsRenderData.m_angleOfRotation = 0.f;
		lv_backgroundStarsRenderData.m_entityPos = glm::vec2{ 0.f, 0.f };
		lv_backgroundStarsRenderData.m_entityTextureHandle = m_backgroundStarsTextureHandle;

		RenderSystem::RenderingData lv_backgroundStars2RenderData{};
		lv_backgroundStars2RenderData.m_angleOfRotation = 0.f;
		lv_backgroundStars2RenderData.m_entityPos = glm::vec2{ 0.f, 0.f };
		lv_backgroundStars2RenderData.m_entityTextureHandle = m_backgroundStars2TextureHandle;

		constexpr float lv_totalSecondsFirstLevel = 75.f;
		constexpr float lv_totalSecondsSecondLevel = 120.f;
		constexpr uint32_t lv_minAsteroidsToHitToGoToSecondLevel = 30U;
		constexpr uint32_t lv_minAsteroidsToHitToWinInSecondLevel = 20U;
		bool lv_isPlayerAlive = true;
		bool lv_timeRewinded{ false };

		while (false == lv_quit) {

			m_trackLastFrameElapsedTime.m_currentTime = SDL_GetTicks();

			m_inputSystem.FlushNotAllowedRepetitionKeys();

			SDL_Event lv_event;
			static uint64_t lv_HideOrShow{};

			while (true == SDL_PollEvent(&lv_event)) {

				ImGui_ImplSDL3_ProcessEvent(&lv_event);
				m_inputSystem.ProcessInput(lv_event, m_window);

				if (true == m_inputSystem.IsKeyUp(InputSystem::Keys::KEY_F1)) {
					if (0 == lv_HideOrShow % 2) {
						show_demo_window = false;

					}
					else {
						show_demo_window = true;

					}
					++lv_HideOrShow;
				}


				if (SDL_EVENT_QUIT == lv_event.type) {
					lv_quit = true;
				}
			}


			assert(true == m_renderer.ClearWindow());
			GetCurrentWindowSize(lv_currentWindowSize);

			lv_backgroundStarsRenderData.m_heightToRender = lv_currentWindowSize.y;
			lv_backgroundStarsRenderData.m_widthToRender = lv_currentWindowSize.x;
			lv_backgroundStarsRenderData.m_centerOfRotation.x = (float)lv_currentWindowSize.x / 2.f;
			lv_backgroundStarsRenderData.m_centerOfRotation.y = (float)lv_currentWindowSize.y / 2.f;



			lv_backgroundStars2RenderData.m_heightToRender = lv_currentWindowSize.y;
			lv_backgroundStars2RenderData.m_widthToRender = lv_currentWindowSize.x;
			lv_backgroundStars2RenderData.m_centerOfRotation.x = (float)lv_currentWindowSize.x / 2.f;
			lv_backgroundStars2RenderData.m_centerOfRotation.y = (float)lv_currentWindowSize.y / 2.f;


			if(1U == m_currentLevel) {
				m_renderer.RenderEntity(lv_backgroundStarsRenderData);

			}
			else {
				m_renderer.RenderEntity(lv_backgroundStars2RenderData);
			}


			bool lv_loopOverInThisLevel = ((m_timeSinceStartInSeconds <= lv_totalSecondsFirstLevel && 1U == m_currentLevel)
				|| (m_timeSinceStartInSeconds <= lv_totalSecondsSecondLevel && 2U == m_currentLevel)) && true == lv_isPlayerAlive;

			if (true == lv_loopOverInThisLevel) {
				if (true == m_inputSystem.IsRepetitionAllowedKeyPressed(InputSystem::Keys::KEY_T) && false == m_inputSystem.IsKeyUp(InputSystem::Keys::KEY_T)) {
					m_timeRewind.RewindTimeByOneFrame(m_entities, m_inputSystem,m_timeSinceStartInSeconds, lv_updateComponent.m_totalNumAsteroidsHitByBullets, m_callbacksTimer);
					lv_timeRewinded = true;
				}
				else {
					m_timeRewind.Update(m_entities, m_inputSystem,m_timeSinceStartInSeconds, lv_updateComponent.m_totalNumAsteroidsHitByBullets, m_callbacksTimer.GetDelayedCallbacks());
					lv_timeRewinded = false;
				}
			}
			else {
				lv_timeRewinded = false;
			}
			
			
			m_callbacksTimer.Update();
			auto* lv_playerAttribComp = (PlayerAttributeComponent*)m_entities[m_playerEntityHandle].GetComponent(ComponentTypes::ATTRIBUTE);
			lv_isPlayerAlive = (0U == lv_playerAttribComp->GetHp()) ? false : true;
			LOG(Severity::INFO, Channel::PROGRAM_LOGIC, "HP: %u", lv_playerAttribComp->GetHp());
			if (true == lv_loopOverInThisLevel) {

				m_grid.Update(lv_currentWindowSize, m_circleBoundsEntities, m_entities);
				m_grid.DoCollisionDetection(m_circleBoundsEntities, m_entities, m_callbacksTimer, m_eventManager, m_allocator);
				m_entitySpawnerFromPools.SpawnNewEntitiesIfConditionsMet(m_currentLevel, lv_timeRewinded);
				lv_updateComponent.m_deltaTime = (float)m_trackLastFrameElapsedTime.m_lastFrameElapsedTime;
				for (auto& l_entity : m_entities) {
					if (true == l_entity.GetActiveState()) {
						assert(l_entity.Update(lv_updateComponent));
					}
				}
				m_eventManager.Update(m_allocator);
				m_entitySpawnerFromPools.UpdatePools();
				UpdateCircleBounds();




				ImGui_ImplSDLRenderer3_NewFrame();
				ImGui_ImplSDL3_NewFrame();
				ImGui::NewFrame();

				{

					auto& lv_player = m_entities[m_playerEntityHandle];

					ImGui::Begin("Player Info");

					ImGui::Text("Score: %u", lv_updateComponent.m_totalNumAsteroidsHitByBullets);
					ImGui::Text("Seconds: %u", (uint32_t)m_timeSinceStartInSeconds);
					ImGui::Text("Health: %u", lv_playerAttribComp->GetHp());

					ImGui::End();
				}
			}
			else {
				
				m_eventManager.FlushAllEventQueues(m_allocator);

				ImGui_ImplSDLRenderer3_NewFrame();
				ImGui_ImplSDL3_NewFrame();
				ImGui::NewFrame();


				if (1U == m_currentLevel) {

					ImGui::Begin("Game status");

					if (lv_minAsteroidsToHitToGoToSecondLevel <= lv_updateComponent.m_totalNumAsteroidsHitByBullets && true == lv_isPlayerAlive) {

						static bool lv_enteredThisLoop{ false };


						ImGui::Text("Congrats! You have successfully completed this level!");
						ImGui::Text("Starting the next level...");

						if (false == lv_enteredThisLoop) {

							m_callbacksTimer.FlushAllCallbacks();
							for (uint32_t i = m_playerEntityHandle + 1; i < (uint32_t)m_entities.size(); ++i) {

								if (EntityType::CURSOR != m_entities[i].GetType()) {
									m_entities[i].SetActiveState(false);
								}

								if (EntityType::ASTEROID == m_entities[i].GetType()) {
									auto* lv_asteroidExp = (OnceRepeatableAnimationComponent*)m_entities[i].GetComponent(ComponentTypes::EXPLOSION_FIRE_ASTEROID_ANIMATION);
									auto* lv_asteroidWarp = (OnceRepeatableAnimationComponent*)m_entities[i].GetComponent(ComponentTypes::WARP_ASTEROID_ANIMATION);

									lv_asteroidWarp->Reset();
									lv_asteroidExp->Reset();
								}

							}
							m_entitySpawnerFromPools.ResetPools();
							m_timeRewind.Flush();
							lv_playerAttribComp->ResetHealth();

							DelayedSetStateCallback lv_exitCallback
							{
								.m_callback{[&]() 
								{
									m_currentLevel += 1U; 
									m_timeSinceStartInSeconds = 0U;
									lv_updateComponent.m_totalNumAsteroidsHitByBullets = 0U;
									lv_enteredThisLoop = false;
								}},
								.m_maxNumFrames = 512U
							};

							m_callbacksTimer.AddSetStateCallback(std::move(lv_exitCallback));
						}

						lv_enteredThisLoop = true;


					}
					else {

						static bool lv_repeat{ false };
						static bool lv_exit{ false };

						if (false == lv_isPlayerAlive) {
							ImGui::Text("You died. Would you like to repeat this level or exit?");
						}
						else {
							ImGui::Text("Failed to clear this level! Would you like to repeat or exit?");
						}
						if (true == ImGui::Button("Repeat")) {
							lv_repeat = true;
						}
						if (true == ImGui::Button("Exit")) {
							lv_exit = true;
						}

						if (true == lv_repeat) {


							m_callbacksTimer.FlushAllCallbacks();
							for (uint32_t i = m_playerEntityHandle + 1; i < (uint32_t)m_entities.size(); ++i) {


								if (EntityType::CURSOR != m_entities[i].GetType()) {
									m_entities[i].SetActiveState(false);
								}

								if (EntityType::ASTEROID == m_entities[i].GetType()) {
									auto* lv_asteroidExp = (OnceRepeatableAnimationComponent*)m_entities[i].GetComponent(ComponentTypes::EXPLOSION_FIRE_ASTEROID_ANIMATION);
									auto* lv_asteroidWarp = (OnceRepeatableAnimationComponent*)m_entities[i].GetComponent(ComponentTypes::WARP_ASTEROID_ANIMATION);

									lv_asteroidWarp->Reset();
									lv_asteroidExp->Reset();
								}

							}
							m_timeRewind.Flush();

							m_entitySpawnerFromPools.ResetPools();
							m_timeSinceStartInSeconds = 0.f;
							lv_updateComponent.m_totalNumAsteroidsHitByBullets = 0U;
							lv_isPlayerAlive = true;
							lv_playerAttribComp->ResetHealth();


							lv_repeat = false;
							lv_exit = false;
						}

						if (true == lv_exit) {
							lv_quit = true;
						}
					}



					ImGui::End();
				}
				else if (2U == m_currentLevel) {
					
					ImGui::Begin("Game status");

					if (lv_minAsteroidsToHitToWinInSecondLevel <= lv_updateComponent.m_totalNumAsteroidsHitByBullets && true == lv_isPlayerAlive) {

						ImGui::Text("You won the game congrats!");
						

						static bool lv_repeat3{ false };
						static bool lv_exit3{ false };

						ImGui::Text("Would you like to play again or exit?");

						if (true == ImGui::Button("Play again")) {

							lv_repeat3 = true;
						}

						if (true == ImGui::Button("Exit")) {

							lv_quit = true;

						}


						if (true == lv_repeat3) {

							m_callbacksTimer.FlushAllCallbacks();
							for (uint32_t i = m_playerEntityHandle + 1; i < (uint32_t)m_entities.size(); ++i) {

								if (EntityType::CURSOR != m_entities[i].GetType()) {
									m_entities[i].SetActiveState(false);
								}

								if (EntityType::ASTEROID == m_entities[i].GetType()) {
									auto* lv_asteroidExp = (OnceRepeatableAnimationComponent*)m_entities[i].GetComponent(ComponentTypes::EXPLOSION_FIRE_ASTEROID_ANIMATION);
									auto* lv_asteroidWarp = (OnceRepeatableAnimationComponent*)m_entities[i].GetComponent(ComponentTypes::WARP_ASTEROID_ANIMATION);

									lv_asteroidWarp->Reset();
									lv_asteroidExp->Reset();
								}

							}
							m_timeRewind.Flush();

							m_entitySpawnerFromPools.ResetPools();
							m_timeSinceStartInSeconds = 0.f;
							lv_updateComponent.m_totalNumAsteroidsHitByBullets = 0U;
							lv_isPlayerAlive = true;
							lv_playerAttribComp->ResetHealth();

							m_currentLevel = 1U;

							lv_repeat3 = false;

						}

					}
					else {

						static bool lv_repeat2{ false };
						static bool lv_exit2{ false };

						if (false == lv_isPlayerAlive) {
							ImGui::Text("You died! Would you like to repeat this level or exit?");
						}
						else {
							ImGui::Text("Failed to clear this level! Would you like to repeat this level or exit?");
						}
						if (true == ImGui::Button("Repeat")) {
							lv_repeat2 = true;
							m_currentLevel = 2U;
						}
						if (true == ImGui::Button("Exit")) {
							lv_exit2 = true;
						}

						if (true == lv_repeat2) {

							m_callbacksTimer.FlushAllCallbacks();
							for (uint32_t i = m_playerEntityHandle + 1; i < (uint32_t)m_entities.size(); ++i) {

								if (EntityType::CURSOR != m_entities[i].GetType()) {
									m_entities[i].SetActiveState(false);
								}
								
								if (EntityType::ASTEROID == m_entities[i].GetType()) {
									auto* lv_asteroidExp = (OnceRepeatableAnimationComponent*)m_entities[i].GetComponent(ComponentTypes::EXPLOSION_FIRE_ASTEROID_ANIMATION);
									auto* lv_asteroidWarp = (OnceRepeatableAnimationComponent*)m_entities[i].GetComponent(ComponentTypes::WARP_ASTEROID_ANIMATION);

									lv_asteroidWarp->Reset();
									lv_asteroidExp->Reset();
								}

							}
							m_timeRewind.Flush();

							m_entitySpawnerFromPools.ResetPools();
							m_timeSinceStartInSeconds = 0.f;
							lv_updateComponent.m_totalNumAsteroidsHitByBullets = 0U;
							lv_isPlayerAlive = true;
							lv_playerAttribComp->ResetHealth();


							lv_repeat2 = false;
							lv_exit2 = false;
						}

						if (true == lv_exit2) {
							lv_quit = true;
						}

					}


					ImGui::End();

				}

			}


			ImGui::Render();
			ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer.GetSDLRenderer());
			assert(true == m_renderer.PresentToWindow());

			m_trackLastFrameElapsedTime.m_lastFrameElapsedTime = SDL_GetTicks() - m_trackLastFrameElapsedTime.m_currentTime;

			if (lv_60fpsInMilliseconds > m_trackLastFrameElapsedTime.m_lastFrameElapsedTime) {

				const uint64_t lv_delayTime = (uint64_t)(lv_60fpsInMilliseconds - m_trackLastFrameElapsedTime.m_lastFrameElapsedTime);
				m_trackLastFrameElapsedTime.m_lastFrameElapsedTime += lv_delayTime;
				SDL_Delay((uint32_t)lv_delayTime);

			}

			m_timeSinceStartInSeconds += ((float)m_trackLastFrameElapsedTime.m_lastFrameElapsedTime/1000.f);
		}

		return true;

	}


	Entity& Engine::GetEntityFromHandle(const EntityHandle l_entityHandle)
	{
		using namespace LogSystem;

		if ((uint32_t)m_entities.size() <= l_entityHandle.m_entityHandle) {
			LOG(Severity::FAILURE, Channel::MEMORY, "An attempt was made to access out of bound memory in vector of entities. The index used: %u\n", l_entityHandle.m_entityHandle);
			throw std::out_of_range("An attempt was made to access out of bound memory in vector of entities.");
		}
		else {
			return m_entities[l_entityHandle.m_entityHandle];
		}
	}
	const Entity& Engine::GetEntityFromHandle(const EntityHandle l_entityHandle) const
	{
		using namespace LogSystem;

		if ((uint32_t)m_entities.size() <= l_entityHandle.m_entityHandle) {
			LOG(Severity::FAILURE, Channel::MEMORY, "An attempt was made to access out of bound memory in vector of entities.\n");
			throw std::out_of_range("An attempt was made to access out of bound memory in vector of entities.");
		}
		else {
			return m_entities[l_entityHandle.m_entityHandle];
		}
	}

	Entity& Engine::GetEntityFromType(const EntityType l_type)
	{
		using namespace LogSystem;

		for (auto& l_entity : m_entities) {
			if (l_type == l_entity.GetType()) {
				return l_entity;
			}
		}

		LOG(Severity::FAILURE, Channel::PROGRAM_LOGIC, "No entity with the requested type exist yet.\n");
		throw std::runtime_error("No entity with the requested type exist yet.");

	}
	const Entity& Engine::GetEntityFromType(const EntityType l_type) const
	{
		using namespace LogSystem;

		for (const auto& l_entity : m_entities) {
			if (l_type == l_entity.GetType()) {
				return l_entity;
			}
		}

		LOG(Severity::FAILURE, Channel::PROGRAM_LOGIC, "No entity with the requested type exist yet.\n");
		throw std::runtime_error("No entity with the requested type exist yet.");
	}


	RenderSystem::Renderer* Engine::GetRenderer()
	{
		return &m_renderer;
	}


	const InputSystem& Engine::GetInputSystem() const
	{
		return m_inputSystem;
	}


	const std::vector<Entity>& Engine::GetEntities() const
	{
		return m_entities;
	}

	void Engine::InitEntitiesAndPools()
	{
		glm::ivec2 lv_windowRes{};
		GetCurrentWindowSize(lv_windowRes);

		m_circleBoundsEntities.reserve(256U);
		m_entities.reserve(256U);

		const auto* lv_warpAsteroidAnimMeta = GetAnimationMeta(AnimationType::WARP_ASTEROID);

		//Main player initialization
		{
			const auto* lv_spaceshipAnimMeta = GetAnimationMeta(AnimationType::MAIN_SPACESHIP);

			assert(nullptr != lv_spaceshipAnimMeta);


			UserInputBasedMovementComponent* lv_movementComponent = static_cast<UserInputBasedMovementComponent*>(m_allocator.Allocate(sizeof(UserInputBasedMovementComponent)));
			PlayerCollisionComponent* lv_collisionComponent = static_cast<PlayerCollisionComponent*>(m_allocator.Allocate(sizeof(PlayerCollisionComponent)));
			IndefiniteRepeatableAnimationComponent* lv_entityAnimationComp = static_cast<IndefiniteRepeatableAnimationComponent*>(m_allocator.Allocate(sizeof(IndefiniteRepeatableAnimationComponent)));
			ActiveBasedStateComponent* lv_activeComponent = static_cast<ActiveBasedStateComponent*>(m_allocator.Allocate(sizeof(ActiveBasedStateComponent)));
			PlayerAttributeComponent* lv_playerAttribComponent = static_cast<PlayerAttributeComponent*>(m_allocator.Allocate(sizeof(PlayerAttributeComponent)));

			
			lv_movementComponent = new(lv_movementComponent) UserInputBasedMovementComponent();
			lv_collisionComponent = new(lv_collisionComponent) PlayerCollisionComponent();
			lv_entityAnimationComp = new(lv_entityAnimationComp) IndefiniteRepeatableAnimationComponent();
			lv_activeComponent = new(lv_activeComponent) ActiveBasedStateComponent();
			lv_playerAttribComponent = new(lv_playerAttribComponent) PlayerAttributeComponent();



			lv_movementComponent->Init(0);
			lv_collisionComponent->Init(0, 0, 0, true, lv_entityAnimationComp, lv_playerAttribComponent, lv_warpAsteroidAnimMeta->m_totalNumFrames + 20U);
			lv_entityAnimationComp->Init(0, lv_spaceshipAnimMeta, lv_movementComponent
										,lv_activeComponent, 0, 0, true);
			lv_activeComponent->Init(0, lv_collisionComponent, lv_entityAnimationComp,
				1, 1);
			lv_playerAttribComponent->Init(0, 10U);
			auto& lv_player = m_entities.emplace_back(std::move(Entity(glm::vec2{ (float)lv_windowRes.x/2.f, (float)lv_windowRes.y/2.f }, 0, EntityType::PLAYER, true)));

			lv_player.AddComponent(ComponentTypes::MOVEMENT, lv_movementComponent);
			lv_player.AddComponent(ComponentTypes::ACTIVE_BASED_STATE, lv_activeComponent);
			lv_player.AddComponent(ComponentTypes::INDEFINITE_ENTITY_ANIMATION, lv_entityAnimationComp);
			lv_player.AddComponent(ComponentTypes::COLLISION, lv_collisionComponent);
			lv_player.AddComponent(ComponentTypes::ATTRIBUTE, lv_playerAttribComponent);
			
			m_playerEntityHandle = 0U;
			
			m_circleBoundsEntities.push_back(Circle{ .m_center{lv_player.GetCurrentPos()}, .m_radius{lv_spaceshipAnimMeta->m_widthToRenderTextures/2.f} });
		}

		//Bullet entities and pool initialization
		{
			constexpr uint32_t lv_totalNumBullets{64U};
			const auto* lv_bulletAnimMetaData = GetAnimationMeta(AnimationType::LASER_BEAM);
			assert(nullptr != lv_bulletAnimMetaData);
			m_entitySpawnerFromPools.InitPool(Asteroid::EntityType::BULLET, (uint32_t)m_entities.size() ,lv_totalNumBullets);
			
			for (uint32_t i = 0U; i < lv_totalNumBullets; ++i) {

				const uint32_t lv_bulletIdx = 1U + i;


				BulletCollisionComponent* lv_collisionComponent = static_cast<BulletCollisionComponent*>(m_allocator.Allocate(sizeof(BulletCollisionComponent)));
				RayMovementComponent* lv_movementComponent = static_cast<RayMovementComponent*>(m_allocator.Allocate(sizeof(RayMovementComponent)));
				ActiveBasedStateComponent* lv_activeComponent = static_cast<ActiveBasedStateComponent*>(m_allocator.Allocate(sizeof(ActiveBasedStateComponent)));
				IndefiniteRepeatableAnimationComponent* lv_entityMainAnimation = static_cast<IndefiniteRepeatableAnimationComponent*>(m_allocator.Allocate(sizeof(IndefiniteRepeatableAnimationComponent)));

				lv_collisionComponent = new(lv_collisionComponent) BulletCollisionComponent();
				lv_movementComponent = new(lv_movementComponent) RayMovementComponent();
				lv_activeComponent = new(lv_activeComponent) ActiveBasedStateComponent();
				lv_entityMainAnimation = new(lv_entityMainAnimation) IndefiniteRepeatableAnimationComponent();


				lv_collisionComponent->Init(lv_bulletIdx,0, 0, true
											, lv_entityMainAnimation, lv_activeComponent);
				lv_movementComponent->Init(lv_bulletIdx);
				lv_activeComponent->Init(lv_bulletIdx, lv_collisionComponent, lv_entityMainAnimation, 0, 0);
				lv_entityMainAnimation->Init(lv_bulletIdx, lv_bulletAnimMetaData, lv_movementComponent, lv_activeComponent, 0, 0, true);

				auto& lv_bullet = m_entities.emplace_back(std::move(Entity(glm::vec2{ 0.f, 0.f }, lv_bulletIdx, EntityType::BULLET, false)));
			

				lv_bullet.AddComponent(ComponentTypes::COLLISION, lv_collisionComponent);
				lv_bullet.AddComponent(ComponentTypes::ACTIVE_BASED_STATE, lv_activeComponent);
				lv_bullet.AddComponent(ComponentTypes::MOVEMENT, lv_movementComponent);
				lv_bullet.AddComponent(ComponentTypes::INDEFINITE_ENTITY_ANIMATION, lv_entityMainAnimation);
				

				
				m_circleBoundsEntities.push_back(Circle{ .m_center{lv_bullet.GetCurrentPos()}, .m_radius{lv_bulletAnimMetaData->m_widthToRenderTextures/2.f} });
				

			}

		}

		//Asteroid Entities and pool initialization
		{
			constexpr uint32_t lv_totalNumAsteroids{ 128U };
			const auto* lv_asteroidAnimMeta = GetAnimationMeta(AnimationType::ASTEROID);
			const auto* lv_explosionAsteroidAnimMeta = GetAnimationMeta(AnimationType::EXPLOSION_FIRE_ASTEROID);
			assert(nullptr != lv_asteroidAnimMeta);
			assert(nullptr != lv_warpAsteroidAnimMeta);
			const uint32_t lv_entitiesLastIndex = (uint32_t)m_entities.size();
			m_entitySpawnerFromPools.InitPool(Asteroid::EntityType::ASTEROID, lv_entitiesLastIndex, lv_totalNumAsteroids);

			for (uint32_t i = 0U; i < lv_totalNumAsteroids; ++i) {

				const uint32_t lv_asteroidIdx = lv_entitiesLastIndex + i;


				AsteroidCollisionComponent* lv_collisionComponent = static_cast<AsteroidCollisionComponent*>(m_allocator.Allocate(sizeof(AsteroidCollisionComponent)));
				ActiveBasedStateComponent* lv_activeComponent = static_cast<ActiveBasedStateComponent*>(m_allocator.Allocate(sizeof(ActiveBasedStateComponent)));
				RayMovementComponent* lv_movementComponent = static_cast<RayMovementComponent*>(m_allocator.Allocate(sizeof(RayMovementComponent)));
				IndefiniteRepeatableAnimationComponent* lv_entityMainAnimation = static_cast<IndefiniteRepeatableAnimationComponent*>(m_allocator.Allocate(sizeof(IndefiniteRepeatableAnimationComponent)));
				OnceRepeatableAnimationComponent* lv_fireExplosionAnimationComponent = static_cast<OnceRepeatableAnimationComponent*>(m_allocator.Allocate(sizeof(OnceRepeatableAnimationComponent)));
				OnceRepeatableAnimationComponent* lv_warpAsteroidAnimComponent = static_cast<OnceRepeatableAnimationComponent*>(m_allocator.Allocate(sizeof(OnceRepeatableAnimationComponent)));
				AsteroidAttributeComponent* lv_asteroidAttribComponent = static_cast<AsteroidAttributeComponent*>(m_allocator.Allocate(sizeof(AsteroidAttributeComponent)));

				lv_collisionComponent = new(lv_collisionComponent) AsteroidCollisionComponent();
				lv_activeComponent = new(lv_activeComponent) ActiveBasedStateComponent();
				lv_movementComponent = new(lv_movementComponent) RayMovementComponent();
				lv_entityMainAnimation = new(lv_entityMainAnimation) IndefiniteRepeatableAnimationComponent();
				lv_fireExplosionAnimationComponent = new(lv_fireExplosionAnimationComponent) OnceRepeatableAnimationComponent();
				lv_warpAsteroidAnimComponent = new(lv_warpAsteroidAnimComponent) OnceRepeatableAnimationComponent();
				lv_asteroidAttribComponent = new(lv_asteroidAttribComponent) AsteroidAttributeComponent();

				lv_collisionComponent->Init(lv_asteroidIdx, lv_warpAsteroidAnimMeta->m_totalNumFrames + 20U, lv_explosionAsteroidAnimMeta->m_totalNumFrames / 3
					, true, lv_entityMainAnimation, lv_fireExplosionAnimationComponent
					, lv_activeComponent);
				lv_activeComponent->Init(lv_asteroidIdx, lv_collisionComponent, lv_entityMainAnimation
										, 1, lv_explosionAsteroidAnimMeta->m_totalNumFrames);
				lv_movementComponent->Init(lv_asteroidIdx);
				lv_entityMainAnimation->Init(lv_asteroidIdx, lv_asteroidAnimMeta, lv_movementComponent
											, lv_activeComponent, lv_warpAsteroidAnimMeta->m_totalNumFrames - 20U, 0, true);
				lv_fireExplosionAnimationComponent->Init(lv_asteroidIdx, lv_explosionAsteroidAnimMeta , lv_movementComponent);
				lv_warpAsteroidAnimComponent->Init(lv_asteroidIdx, lv_warpAsteroidAnimMeta, lv_movementComponent, false);

				lv_asteroidAttribComponent->Init(lv_asteroidIdx, 3, AsteroidStates::PASSIVE, lv_movementComponent);

				auto& lv_asteroid = m_entities.emplace_back(std::move(Entity(glm::vec2{ 0.f, 0.f }, lv_asteroidIdx,  EntityType::ASTEROID, false)));

				lv_asteroid.AddComponent(ComponentTypes::COLLISION, lv_collisionComponent);
				lv_asteroid.AddComponent(ComponentTypes::ACTIVE_BASED_STATE, lv_activeComponent);
				lv_asteroid.AddComponent(ComponentTypes::MOVEMENT, lv_movementComponent);
				lv_asteroid.AddComponent(ComponentTypes::INDEFINITE_ENTITY_ANIMATION, lv_entityMainAnimation);
				lv_asteroid.AddComponent(ComponentTypes::EXPLOSION_FIRE_ASTEROID_ANIMATION, lv_fireExplosionAnimationComponent);
				lv_asteroid.AddComponent(ComponentTypes::WARP_ASTEROID_ANIMATION, lv_warpAsteroidAnimComponent);
				lv_asteroid.AddComponent(ComponentTypes::ATTRIBUTE, lv_asteroidAttribComponent);


				m_circleBoundsEntities.push_back(Circle{ .m_center{lv_asteroid.GetCurrentPos()}, .m_radius{lv_asteroidAnimMeta->m_widthToRenderTextures/2.f} });


			}
		}


		{

			const auto* lv_cursorAnim = GetAnimationMeta(AnimationType::CURSOR);
			auto lv_index = m_entities.size();

			RayMovementComponent* lv_movementComp = static_cast<RayMovementComponent*>(m_allocator.Allocate(sizeof(RayMovementComponent)));
			IndefiniteRepeatableAnimationComponent* lv_mainAnimComp = static_cast<IndefiniteRepeatableAnimationComponent*>(m_allocator.Allocate(sizeof(IndefiniteRepeatableAnimationComponent)));
			CursorAttributeComponent* lv_cursorAttribComp = static_cast<CursorAttributeComponent*>(m_allocator.Allocate(sizeof(CursorAttributeComponent)));

			lv_movementComp = new(lv_movementComp) RayMovementComponent();
			lv_mainAnimComp = new(lv_mainAnimComp) IndefiniteRepeatableAnimationComponent();
			lv_cursorAttribComp = new(lv_cursorAttribComp) CursorAttributeComponent();

			lv_movementComp->Init(static_cast<EntityHandle>(lv_index));
			lv_mainAnimComp->Init(static_cast<EntityHandle>(lv_index), lv_cursorAnim, lv_movementComp, nullptr, 0, 0);
			lv_cursorAttribComp->Init(static_cast<EntityHandle>(lv_index), 0, lv_movementComp, lv_mainAnimComp);

			auto& lv_cursor = m_entities.emplace_back(std::move(Entity(glm::vec2{ 0.f, 0.f }, lv_index, EntityType::CURSOR, true)));

			lv_cursor.AddComponent(ComponentTypes::ATTRIBUTE, lv_cursorAttribComp);
			lv_cursor.AddComponent(ComponentTypes::MOVEMENT, lv_movementComp);
			lv_cursor.AddComponent(ComponentTypes::INDEFINITE_ENTITY_ANIMATION, lv_mainAnimComp);

			m_circleBoundsEntities.push_back(Circle{ .m_center{lv_cursor.GetCurrentPos()}, .m_radius{32.f}});


		}

	}

	void Engine::UpdateCircleBounds()
	{
		for (size_t i = 0; i < m_entities.size(); ++i) {
			if (true == m_entities[i].GetActiveState()) {
				m_circleBoundsEntities[i].m_center = m_entities[i].GetCurrentPos();
			}
		}
	}

	void Engine::GetCurrentWindowSize(glm::ivec2& l_windowRes) const
	{
		using namespace LogSystem;

		bool lv_result = SDL_GetWindowSize(m_window, &l_windowRes.x, &l_windowRes.y);

		if (false == lv_result) {
			
			LOG(Severity::FAILURE, Channel::GRAPHICS, "Failed to get current window size: %s", SDL_GetError());
			
			throw std::runtime_error("Failed to get current window size");
		}

	}


	CallbacksTimer& Engine::GetCallbacksTimer()
	{
		return m_callbacksTimer;
	}


	const AnimationMetaData* Engine::GetAnimationMeta(const AnimationType l_type) const
	{
		for (const auto& l_metaData : m_animationMetaData) {
			if (l_type == l_metaData.m_type) {
				return &l_metaData;
			}
		}

		return nullptr;
	}

	const std::vector<Circle>& Engine::GetCircleBounds() const
	{
		return m_circleBoundsEntities;
	}


	const Grid& Engine::GetGrid() const
	{
		return m_grid;
	}


	Engine::~Engine()
	{

		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();

		SDL_DestroyWindow(m_window);
	}
}