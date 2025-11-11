import mongoose from "mongoose";

const userSchema = new mongoose.Schema({
  name: { 
    type: String, 
    required: [true, "Name is required"],
    trim: true,
    minlength: [2, "Name must be at least 2 characters"],
    maxlength: [50, "Name cannot exceed 50 characters"]
  },
  email: { 
    type: String, 
    required: [true, "Email is required"],
    unique: true,
    lowercase: true,
    trim: true,
    match: [/^\w+([.-]?\w+)*@\w+([.-]?\w+)*(\.\w{2,3})+$/, "Please enter a valid email"]
  },
  phone: {
    type: String,
    trim: true,
    sparse: true
  },
  userType: {
    type: String,
    enum: ["rider", "driver", "admin"],
    default: "rider"
  },
  isActive: {
    type: Boolean,
    default: true
  },
  profileCompleted: {
    type: Boolean,
    default: false
  }
}, {
  timestamps: true // Adds createdAt and updatedAt automatically
});

// Create index for better query performance
userSchema.index({ email: 1 });
userSchema.index({ userType: 1 });
userSchema.index({ createdAt: -1 });

// Static method to find active users
userSchema.statics.findActiveUsers = function() {
  return this.find({ isActive: true });
};

// Instance method to get user profile summary
userSchema.methods.getProfileSummary = function() {
  return {
    id: this._id,
    name: this.name,
    email: this.email,
    userType: this.userType,
    profileCompleted: this.profileCompleted
  };
};

const User = mongoose.model("User", userSchema);

export default User;
