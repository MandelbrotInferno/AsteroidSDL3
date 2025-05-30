



#include "Components/MovementComponent.hpp"
#include <gtc/matrix_transform.hpp>


namespace Asteroid
{

	MovementComponent::MovementComponent()
	{

	}


	const glm::vec2& MovementComponent::GetSpeed() const
	{
		return m_speed;
	}



	void MovementComponent::Init(EntityHandle l_ownerEntityHandle)
	{
		Component::Init(l_ownerEntityHandle);
	}


	void MovementComponent::SetSpeed(const glm::vec2& l_newSpeed)
	{
		m_speed = l_newSpeed;
	}

	float MovementComponent::GetCurrentAngleOfRotation() const
	{
		return m_thetaDegrees;
	}

	void MovementComponent::SetAngleOfRotation(const float l_theta)
	{
		m_thetaDegrees = l_theta;
	}


	bool MovementComponent::GetPauseState() const
	{
		return m_pauseMovement;
	}

	void MovementComponent::SetPauseState(const bool l_pauseState)
	{
		m_pauseMovement = l_pauseState;
	}
}