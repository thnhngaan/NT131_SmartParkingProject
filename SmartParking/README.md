# Smart Parking Dashboard

A scalable web dashboard for managing smart parking systems built with Node.js, Express.js, and MongoDB.

## Features

- User authentication with JWT (signup, signin, signout)
- Role-based access (Admin and regular users)
- Dashboard with Tailwind CSS styling
- Parking slot management (view, add, edit, delete for admins)
- Real-time updates (simulated with polling)
- Analytics (total, occupied, available slots)
- RESTful API
- MVC architecture
- Environment configuration

## Project Structure

```
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
```

## Installation

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd smartparking
   ```

2. Install dependencies:
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

## Usage

1. Open http://localhost:3000 in your browser
2. Sign up as a user or admin
3. Login to access the dashboard
4. Admins can add/manage parking slots
5. View real-time slot status and analytics

## API Endpoints

### Authentication
- POST /api/auth/signup - User registration
- POST /api/auth/signin - User login
- POST /api/auth/signout - User logout
- GET /api/auth/me - Get current user info

### Parking
- GET /api/parking - Get all parking slots
- POST /api/parking - Add new slot (admin only)
- PUT /api/parking/:id - Update slot (admin only)
- DELETE /api/parking/:id - Delete slot (admin only)
- GET /api/parking/analytics - Get analytics

## Technologies Used

- Node.js
- Express.js
- MongoDB with Mongoose
- JWT for authentication
- EJS for templating
- Tailwind CSS for styling
- bcryptjs for password hashing

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Open a Pull Request

## License

ISC