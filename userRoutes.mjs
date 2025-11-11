import express from "express";
import User from "../models/User.mjs";

const router = express.Router();

console.log("✅ User routes module loaded");

// SINGLE MULTI-PURPOSE USER API ENDPOINT
router.post("/operations", async (req, res) => {
  try {
    console.log("🎯 /api/users/operations endpoint HIT!");
    console.log("Request body:", req.body);
    
    const { action, data, userId, page = 1, limit = 10, search, userType } = req.body;

    console.log(`🔧 Action requested: ${action}`);

    switch (action) {
      // GET all users
      case "getAll":
        console.log("📋 Getting all users...");
        let query = {};
        
        if (search) {
          query.$or = [
            { name: { $regex: search, $options: 'i' } },
            { email: { $regex: search, $options: 'i' } }
          ];
        }

        if (userType) {
          query.userType = userType;
        }

        const users = await User.find(query)
          .sort({ createdAt: -1 })
          .limit(limit * 1)
          .skip((page - 1) * limit);

        const total = await User.countDocuments(query);

        console.log(`✅ Found ${users.length} users`);
        return res.json({
          success: true,
          data: users,
          pagination: {
            current: parseInt(page),
            totalPages: Math.ceil(total / limit),
            totalUsers: total,
            hasNext: page < Math.ceil(total / limit),
            hasPrev: page > 1
          },
          type: "users_list"
        });

      // CREATE user
      case "create":
        console.log("👤 Creating new user...");
        const { name, email, phone, userType: newUserType } = data;

        if (!name || !email) {
          return res.status(400).json({
            success: false,
            error: "Name and email are required"
          });
        }

        const existingUser = await User.findOne({ email });
        if (existingUser) {
          return res.status(409).json({
            success: false,
            error: "User with this email already exists"
          });
        }

        const newUser = new User({
          name,
          email,
          phone,
          userType: newUserType || "rider"
        });

        await newUser.save();

        console.log(`✅ User created: ${newUser.name}`);
        return res.json({
          success: true,
          data: newUser,
          message: "User created successfully",
          type: "user_created"
        });

      // LOGIN user
      case "login":
        console.log("🔐 Login attempt...");
        const { email: loginEmail, password } = data;
        
        if (!loginEmail || !password) {
          return res.status(400).json({
            success: false,
            error: "Email and password are required"
          });
        }

        const user = await User.findOne({ email: loginEmail });
        if (!user) {
          console.log("❌ User not found:", loginEmail);
          return res.status(401).json({
            success: false,
            error: "Invalid email or password"
          });
        }

        // Simple password check - any password works
        console.log(`✅ Login successful for: ${user.name}`);
        return res.json({
          success: true,
          data: user,
          message: "Login successful",
          type: "login_success"
        });

      // GET user by ID
      case "getById":
        console.log(`🔍 Getting user by ID: ${userId}`);
        const userById = await User.findById(userId);
        if (!userById) {
          return res.status(404).json({
            success: false,
            error: "User not found"
          });
        }

        return res.json({
          success: true,
          data: userById,
          type: "user_details"
        });

      default:
        console.log("❌ Invalid action requested:", action);
        return res.status(400).json({ 
          success: false, 
          error: "Invalid action. Use: 'getAll', 'create', 'login', or 'getById'" 
        });
    }

  } catch (err) {
    console.error("❌ User operations error:", err);
    res.status(500).json({
      success: false,
      error: "Server error: " + err.message
    });
  }
});

export default router;