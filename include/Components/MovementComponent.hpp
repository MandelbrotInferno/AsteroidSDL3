#pragma once




#include <glm.hpp>
#include "Components/Component.hpp"
#include <gtc/matrix_transform.hpp>




namespace Asteroid
{

	class MovementComponent : public Component
	{
	public:
		
		MovementComponent();

		virtual ~MovementComponent() = default;

		const glm::mat3& GetTransform() const;

		void SetSpeed(const glm::vec2& l_newSpeed);

		float GetCurrentAngleOfRotation() const;

		const glm::vec2& GetSpeed() const;

		void SetAngleOfRotation(const float l_theta);

		void Init(EntityHandle l_ownerEntityHandle);

		void SetPauseState(const bool l_pauseState);

		bool GetPauseState() const;

	protected:
		glm::mat3 m_transform{ glm::identity<glm::mat3>() };
		float m_thetaDegrees{};
		
		bool m_pauseMovement{ false };

		//Speed along X and Y axis
		glm::vec2 m_speed{0.f};
	};
}