#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
private:

    void updateViewMatrix()
    {
        glm::mat4 currentMatrix = matrices.view;

        glm::mat4 rotM = glm::mat4(1.0f);
        glm::mat4 transM;

        rotM = glm::rotate(rotM, glm::radians(rotation.x * (flipY ? -1.0f : 1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
        rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

        glm::vec3 translation = position;
        translation.x *= -1.0f;
        translation.z *= -1.0f;
        if (flipY) {
            translation.y *= -1.0f;
        }
        transM = glm::translate(glm::mat4(1.0f), translation);

        if (type == CameraType::firstperson)
        {
            matrices.view = rotM * transM;
        }
        else
        {
            matrices.view = transM * rotM;
        }

        viewPos = glm::vec4(translation, 1.0f);

        if (matrices.view != currentMatrix) {
            updated = true;
        }
    };
public:
    enum CameraType { lookat, firstperson };
    CameraType type = CameraType::lookat;

    glm::vec3 rotation = glm::vec3();
    glm::vec3 position = glm::vec3();
    glm::vec4 viewPos = glm::vec4();
    glm::vec3 camFront = glm::vec3();
    glm::vec3 camRight = glm::vec3();
    glm::vec3 camUp = glm::vec3();
    float fov;
    float znear, zfar;
    float aspect = 1;

	bool enableDOF = false; // 是否启用景深效果
    float focusDistance;    // 焦点距离
    float focusRange;       // 焦点范围（清晰区域）
	float maxBlurRadius;    // 最大模糊半径，配合光圈大小，值越大，模糊效果越明显，但性能开销也越大
    float aperture;         // 光圈大小（影响模糊强度）

    float rotationSpeed = 1.0f;
    float movementSpeed = 1.0f;

    bool updated = true;
    bool flipY = true;

    struct
    {
        glm::mat4 perspective;  // 也用于存储正交投影矩阵
        glm::mat4 view;
    } matrices;

    // 正交投影参数
    float orthoLeft = -10, orthoRight = 10;
    float orthoBottom = -10, orthoTop = 10;
    bool useOrthographic = false;  // 标识当前是否使用正交投影

    struct
    {
        bool left = false;		// key A
        bool right = false;		// key D
        bool up = false;		// key W
        bool down = false;		// key S
        bool top = false;		// key Q
        bool bottom = false;	// key E
    } keys;

    bool moving() const
    {
        return keys.left || keys.right || keys.up || keys.down || keys.top || keys.bottom;
    }

    float getNearClip() const {
        return znear;
    }

    float getFarClip() const {
        return zfar;
    }

    void setPerspective(float fov, float aspect, float znear, float zfar)
    {
        this->aspect = aspect;
        useOrthographic = false;
        glm::mat4 currentMatrix = matrices.perspective;
        this->fov = fov;
        this->znear = znear;
        this->zfar = zfar;
        matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
        if (flipY) {
            matrices.perspective[1][1] *= -1.0f;
        }
        if (matrices.perspective != currentMatrix) {
            updated = true;
        }
    };

    // 添加正交投影设置方法
    void setOrthographic(float left, float right, float bottom, float top, float znear, float zfar)
    {
        useOrthographic = true;
        glm::mat4 currentMatrix = matrices.perspective;

        // 保存正交投影参数
        this->orthoLeft = left;
        this->orthoRight = right;
        this->orthoBottom = bottom;
        this->orthoTop = top;
        this->znear = znear;
        this->zfar = zfar;

        // 计算正交投影矩阵
        matrices.perspective = glm::ortho(left, right, bottom, top, znear, zfar);
        if (flipY) {
            matrices.perspective[1][1] *= -1.0f;
        }

        if (matrices.perspective != currentMatrix) {
            updated = true;
        }
    }

    void updateAspectRatio(float aspect)
    {
        glm::mat4 currentMatrix = matrices.perspective;
		this->aspect = aspect;
        if (useOrthographic) {
            matrices.perspective = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, znear, zfar);
        }
        else {
            // 透视投影的处理保持不变
            matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
        }

        if (flipY) {
            matrices.perspective[1][1] *= -1.0f;
        }

        if (matrices.perspective != currentMatrix) {
            updated = true;
        }
    }

    // 切换投影类型的方法
    void switchProjectionType()
    {
        if (useOrthographic) {
            setOrthographic(orthoLeft, orthoRight, orthoBottom, orthoTop, znear, zfar);
        }
        else {
            setPerspective(fov, aspect, znear, zfar);
        }
    }

    void setPosition(glm::vec3 position)
    {
        this->position = position;
        updateViewMatrix();
    }

    void setRotation(glm::vec3 rotation)
    {
        this->rotation = rotation;
        updateViewMatrix();
    }

    void rotate(glm::vec3 delta)
    {
        this->rotation += delta;
        updateViewMatrix();
    }

    void setTranslation(glm::vec3 translation)
    {
        this->position = translation;
        updateViewMatrix();
    };

    void Translate(glm::vec3 delta)
    {
        this->position += delta;
        updateViewMatrix();
    }

    void setRotationSpeed(float rotationSpeed)
    {
        this->rotationSpeed = rotationSpeed;
    }

    void setMovementSpeed(float movementSpeed)
    {
        this->movementSpeed = movementSpeed;
    }
    
    glm::vec3 GetFront()
    {
        return camFront;
    }
    
    glm::vec3 GetUp()
    {
        return camUp;
    }

	glm::vec3 GetRight()
	{
		return camRight;
	}
    void update(float deltaTime)
    {
		camFront.x = -cos(glm::radians(rotation.x * (flipY ? -1.0f : 1.0f))) * sin(glm::radians(rotation.y));
		camFront.y = sin(glm::radians(rotation.x * (flipY ? -1.0f : 1.0f)));
		camFront.z = cos(glm::radians(rotation.x * (flipY ? -1.0f : 1.0f))) * cos(glm::radians(rotation.y));
		camFront = glm::normalize(camFront);
		camRight = glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f)));
		camUp = glm::normalize(glm::cross(camFront, camRight));

        updated = false;
        if (type == CameraType::firstperson)
        {
            if (moving())
            {
                float moveSpeed = deltaTime * movementSpeed;
                if (keys.up)
                    position -= camFront * moveSpeed;
                if (keys.down)
                    position += camFront * moveSpeed;
                if (keys.left)
                    position += camRight * moveSpeed;
                if (keys.right)
                    position -= camRight * moveSpeed;
                if (keys.top)
                    position -= camUp * moveSpeed;
                if (keys.bottom)
                    position += camUp * moveSpeed;
            }
        }
        updateViewMatrix();
    };

};
