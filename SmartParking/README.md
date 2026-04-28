# Smart Parking Dashboard
A web dashboard for an IoT-based smart parking system tailored for the NT131 course, utilizing Node.js and MongoDB for scalable management.

## WebDashboard Structure
smartparking/
├── config/
│   └── database.js
├── controllers/
│   ├── authController.js
│   └── parkingController.js
├── middleware/
│   ├── auth.js
│   └── error.js
├── models/
│   ├── User.js
│   └── ParkingSlot.js
├── routes/
│   ├── auth.js
│   └── parking.js
├── views/
│   ├── partials/
│   │   └── header.ejs
│   ├── index.ejs
│   ├── login.ejs
│   ├── signup.ejs
│   └── dashboard.ejs
├── public/
│   ├── css/
│   ├── js/
│   └── images/
├── .env
├── package.json
├── server.js
└── README.md


## Installation

1. Clone this repository

2. Install dependencies
   ```bash
   npm install
   ```

3. Set up environment variables:
   Create a `.env` file in the root directory with:
   ```
   PORT=3000
   MONGODB_URI=mongodb://localhost:27017/smartparking
   JWT_SECRET=your_jwt_secret_here
   ```

4. Start MongoDB:
   Make sure MongoDB is running on your system.

5. Run the application:
   ```bash
   npm run dev
   ```
   The server will start on http://localhost:3000

